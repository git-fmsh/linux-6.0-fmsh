/*
 *  Based on drivers/serial/8250.c by Russell King.
 *
 *  Author:	wang fengbo
 *  Created:	2018/06/19
 *  Copyright:	(C) 2018 FMSH Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */

#define DEBUG
#if defined(CONFIG_SERIAL_FMQL_PS_UART_CONSOLE) && defined(CONFIG_MAGIC_SYSRQ)
#define SUPPORT_SYSRQ
#endif

#include <linux/module.h>
#include <linux/ioport.h>
#include <linux/init.h>
#include <linux/console.h>
#include <linux/sysrq.h>
#include <linux/serial_reg.h>
#include <linux/circ_buf.h>
#include <linux/delay.h>
#include <linux/interrupt.h>
#include <linux/of.h>
#include <linux/platform_device.h>
#include <linux/tty.h>
#include <linux/tty_flip.h>
#include <linux/serial_core.h>
#include <linux/clk.h>
#include <linux/io.h>
#include <linux/slab.h>

#define FUARTPS_TTY_NAME	"ttyPS"
#define FUARTPS_NAME		"fuartps"
#define FUARTPS_NR_PORTS    2
#define FUARTPS_FIFO_SIZE	64	/* FIFO size */

#define FMSH_NAME_LEN		8
#define UART_DLF	(0x30) /* Divisor Latch Fraction Register 0xC0/4 = 0x30 */
#define UART_USR	(0x1F) /* UART Status Register 0x7C/4 = 0x1F */
#define USR_BUSY	(0x01)

struct uart_fmsh_port {
	struct uart_port        port;
	unsigned char           ier;
	unsigned char           lcr;
	unsigned char           mcr;
	unsigned int            lsr_break_flag;
	struct clk		*clk;
	struct clk		*pclk;
	char			name[FMSH_NAME_LEN];
};

static inline unsigned int serial_in(struct uart_fmsh_port *up, int offset)
{
	unsigned int val;
	offset <<= 2;
	val = readl(up->port.membase + offset);
	return val;
}

static inline void serial_out(struct uart_fmsh_port *up, int offset, int value)
{
	offset <<= 2;
	writel(value, up->port.membase + offset);
}

static void fuartps_enable_ms(struct uart_port *port)
{
	struct uart_fmsh_port *up = (struct uart_fmsh_port *)port;

	up->ier |= UART_IER_MSI;
	serial_out(up, UART_IER, up->ier);
}

static void fuartps_stop_tx(struct uart_port *port)
{
	struct uart_fmsh_port *up = (struct uart_fmsh_port *)port;

	if (up->ier & UART_IER_THRI) {
		up->ier &= ~UART_IER_THRI;
		serial_out(up, UART_IER, up->ier);
	}
}

static void fuartps_stop_rx(struct uart_port *port)
{
	struct uart_fmsh_port *up = (struct uart_fmsh_port *)port;

	up->ier &= ~UART_IER_RLSI;
	up->port.read_status_mask &= ~UART_LSR_DR;
	serial_out(up, UART_IER, up->ier);
}

static inline void receive_chars(struct uart_fmsh_port *up, int *status)
{
	unsigned int ch, flag;
	int max_count = 256;

	do {
		/* work around Errata #20 according to
		 * Intel(R) PXA27x Processor Family
		 * Specification Update (May 2005)
		 *
		 * Step 2
		 * Disable the Reciever Time Out Interrupt via IER[RTOEI]
		 */
		up->ier &= ~UART_IER_RTOIE;
		serial_out(up, UART_IER, up->ier);

		ch = serial_in(up, UART_RX);
		flag = TTY_NORMAL;
		up->port.icount.rx++;

		if (unlikely(*status & (UART_LSR_BI | UART_LSR_PE |
				       UART_LSR_FE | UART_LSR_OE))) {
			/*
			 * For statistics only
			 */
			if (*status & UART_LSR_BI) {
				*status &= ~(UART_LSR_FE | UART_LSR_PE);
				up->port.icount.brk++;
				/*
				 * We do the SysRQ and SAK checking
				 * here because otherwise the break
				 * may get masked by ignore_status_mask
				 * or read_status_mask.
				 */
				if (uart_handle_break(&up->port))
					goto ignore_char;
			} else if (*status & UART_LSR_PE)
				up->port.icount.parity++;
			else if (*status & UART_LSR_FE)
				up->port.icount.frame++;
			if (*status & UART_LSR_OE)
				up->port.icount.overrun++;

			/*
			 * Mask off conditions which should be ignored.
			 */
			*status &= up->port.read_status_mask;

#ifdef CONFIG_SERIAL_FMQL_PS_UART_CONSOLE
			if (up->port.line == up->port.cons->index) {
				/* Recover the break flag from console xmit */
				*status |= up->lsr_break_flag;
				up->lsr_break_flag = 0;
			}
#endif
			if (*status & UART_LSR_BI) {
				flag = TTY_BREAK;
			} else if (*status & UART_LSR_PE)
				flag = TTY_PARITY;
			else if (*status & UART_LSR_FE)
				flag = TTY_FRAME;
		}

		if (uart_handle_sysrq_char(&up->port, ch))
			goto ignore_char;

		uart_insert_char(&up->port, *status, UART_LSR_OE, ch, flag);

	ignore_char:
		*status = serial_in(up, UART_LSR);
	} while ((*status & UART_LSR_DR) && (max_count-- > 0));
	tty_flip_buffer_push(&up->port.state->port);

	/* work around Errata #20 according to
	 * Intel(R) PXA27x Processor Family
	 * Specification Update (May 2005)
	 *
	 * Step 6:
	 * No more data in FIFO: Re-enable RTO interrupt via IER[RTOIE]
	 */
	up->ier |= UART_IER_RTOIE;
	serial_out(up, UART_IER, up->ier);
}

static void transmit_chars(struct uart_fmsh_port *up)
{
	struct circ_buf *xmit = &up->port.state->xmit;
	int count;

	if (up->port.x_char) {
		serial_out(up, UART_TX, up->port.x_char);
		up->port.icount.tx++;
		up->port.x_char = 0;
		return;
	}
	if (uart_circ_empty(xmit) || uart_tx_stopped(&up->port)) {
		fuartps_stop_tx(&up->port);
		return;
	}

	count = up->port.fifosize / 2;
	do {
		serial_out(up, UART_TX, xmit->buf[xmit->tail]);
		xmit->tail = (xmit->tail + 1) & (UART_XMIT_SIZE - 1);
		up->port.icount.tx++;
		if (uart_circ_empty(xmit))
			break;
	} while (--count > 0);

	if (uart_circ_chars_pending(xmit) < WAKEUP_CHARS)
		uart_write_wakeup(&up->port);


	if (uart_circ_empty(xmit))
		fuartps_stop_tx(&up->port);
}

static void fuartps_start_tx(struct uart_port *port)
{
	struct uart_fmsh_port *up = (struct uart_fmsh_port *)port;

	if (!(up->ier & UART_IER_THRI)) {
		up->ier |= UART_IER_THRI;
		serial_out(up, UART_IER, up->ier);
	}
}

static inline void check_modem_status(struct uart_fmsh_port *up)
{
	int status;

	status = serial_in(up, UART_MSR);

	if ((status & UART_MSR_ANY_DELTA) == 0)
		return;

	if (status & UART_MSR_TERI)
		up->port.icount.rng++;
	if (status & UART_MSR_DDSR)
		up->port.icount.dsr++;
	if (status & UART_MSR_DDCD)
		uart_handle_dcd_change(&up->port, status & UART_MSR_DCD);
	if (status & UART_MSR_DCTS)
		uart_handle_cts_change(&up->port, status & UART_MSR_CTS);

	wake_up_interruptible(&up->port.state->port.delta_msr_wait);
}

/*
 * This handles the interrupt from one port.
 */
static irqreturn_t fuartps_irq(int irq, void *dev_id)
{
	struct uart_fmsh_port *up = dev_id;
	unsigned int iir, lsr;

	iir = serial_in(up, UART_IIR);
	if (iir & UART_IIR_NO_INT)
		return IRQ_NONE;
	lsr = serial_in(up, UART_LSR);
	if (lsr & UART_LSR_DR)
		receive_chars(up, &lsr);
	check_modem_status(up);
	if (lsr & UART_LSR_THRE)
		transmit_chars(up);
	return IRQ_HANDLED;
}

static irqreturn_t fuartps_irq1(int irq, void *dev_id)
{
	struct uart_fmsh_port *up = dev_id;
	unsigned int iir, lsr;

	printk("Debug %s %d, 63 irq\n", __func__, __LINE__);
	iir = serial_in(up, UART_IIR);
	if (iir & UART_IIR_NO_INT)
		return IRQ_NONE;
	lsr = serial_in(up, UART_LSR);
	if (lsr & UART_LSR_DR)
		receive_chars(up, &lsr);
	check_modem_status(up);
	if (lsr & UART_LSR_THRE)
		transmit_chars(up);
	return IRQ_HANDLED;
}

static unsigned int fuartps_tx_empty(struct uart_port *port)
{
	struct uart_fmsh_port *up = (struct uart_fmsh_port *)port;
	unsigned long flags;
	unsigned int ret;

	spin_lock_irqsave(&up->port.lock, flags);
	ret = serial_in(up, UART_LSR) & UART_LSR_TEMT ? TIOCSER_TEMT : 0;
	spin_unlock_irqrestore(&up->port.lock, flags);

	return ret;
}

static unsigned int fuartps_get_mctrl(struct uart_port *port)
{
	struct uart_fmsh_port *up = (struct uart_fmsh_port *)port;
	unsigned char status;
	unsigned int ret;

	status = serial_in(up, UART_MSR);

	ret = 0;
	if (status & UART_MSR_DCD)
		ret |= TIOCM_CAR;
	if (status & UART_MSR_RI)
		ret |= TIOCM_RNG;
	if (status & UART_MSR_DSR)
		ret |= TIOCM_DSR;
	if (status & UART_MSR_CTS)
		ret |= TIOCM_CTS;
	return ret;
}

static void fuartps_set_mctrl(struct uart_port *port, unsigned int mctrl)
{
	struct uart_fmsh_port *up = (struct uart_fmsh_port *)port;
	unsigned char mcr = 0;

	if (mctrl & TIOCM_RTS)
		mcr |= UART_MCR_RTS;
	if (mctrl & TIOCM_DTR)
		mcr |= UART_MCR_DTR;
	if (mctrl & TIOCM_OUT1)
		mcr |= UART_MCR_OUT1;
	if (mctrl & TIOCM_OUT2)
		mcr |= UART_MCR_OUT2;
	if (mctrl & TIOCM_LOOP)
		mcr |= UART_MCR_LOOP;

	mcr |= up->mcr;

	serial_out(up, UART_MCR, mcr);
}

static void fuartps_break_ctl(struct uart_port *port, int break_state)
{
	struct uart_fmsh_port *up = (struct uart_fmsh_port *)port;
	unsigned long flags;

	spin_lock_irqsave(&up->port.lock, flags);
	if (break_state == -1)
		up->lcr |= UART_LCR_SBC;
	else
		up->lcr &= ~UART_LCR_SBC;
	serial_out(up, UART_LCR, up->lcr);
	spin_unlock_irqrestore(&up->port.lock, flags);
}

static int s_irq1;
static int fuartps_startup(struct uart_port *port)
{
	struct uart_fmsh_port *up = (struct uart_fmsh_port *)port;
	unsigned long flags;
	int retval;

	printk("Debug %s %d, uart startup\n", __func__, __LINE__);
	if (port->line == 3) /* HWUART */
		up->mcr |= UART_MCR_AFE;
	else
		up->mcr = 0;

	up->port.uartclk = clk_get_rate(up->clk);

	/*
	 * Allocate the IRQ
	 */
	retval = request_irq(up->port.irq, fuartps_irq, IRQF_SHARED, up->name, up);
	if (retval)
		return retval;

	retval = request_irq(s_irq1, fuartps_irq1, IRQF_SHARED, up->name, up);
	if (retval)
		return retval;

	/*
	 * Clear the FIFO buffers and disable them.
	 * (they will be reenabled in set_termios())
	 */
	serial_out(up, UART_FCR, UART_FCR_ENABLE_FIFO);
	serial_out(up, UART_FCR, UART_FCR_ENABLE_FIFO |
			UART_FCR_CLEAR_RCVR | UART_FCR_CLEAR_XMIT);
	serial_out(up, UART_FCR, 0);

	/*
	 * Clear the interrupt registers.
	 */
	(void) serial_in(up, UART_LSR);
	(void) serial_in(up, UART_RX);
	(void) serial_in(up, UART_IIR);
	(void) serial_in(up, UART_MSR);

	/*
	 * Now, initialize the UART
	 */
	serial_out(up, UART_LCR, UART_LCR_WLEN8);

	spin_lock_irqsave(&up->port.lock, flags);
	up->port.mctrl |= TIOCM_OUT2;
	fuartps_set_mctrl(&up->port, up->port.mctrl);
	spin_unlock_irqrestore(&up->port.lock, flags);

	/*
	 * Finally, enable interrupts.  Note: Modem status interrupts
	 * are set via set_termios(), which will be occurring imminently
	 * anyway, so we don't enable them here.
	 */
	up->ier = UART_IER_RLSI | UART_IER_RDI | UART_IER_RTOIE | UART_IER_UUE;
	serial_out(up, UART_IER, up->ier);

	/*
	 * And clear the interrupt REgisters again for luck.
	 */
	(void) serial_in(up, UART_LSR);
	(void) serial_in(up, UART_RX);
	(void) serial_in(up, UART_IIR);
	(void) serial_in(up, UART_MSR);

	return 0;
}

static void fuartps_shutdown(struct uart_port *port)
{
	struct uart_fmsh_port *up = (struct uart_fmsh_port *)port;
	unsigned long flags;

	printk("Debug %s %d, uart startup\n", __func__, __LINE__);
	free_irq(up->port.irq, up);

	/*
	 * Disable interrupts from this port
	 */
	up->ier = 0;
	serial_out(up, UART_IER, 0);

	spin_lock_irqsave(&up->port.lock, flags);
	up->port.mctrl &= ~TIOCM_OUT2;
	fuartps_set_mctrl(&up->port, up->port.mctrl);
	spin_unlock_irqrestore(&up->port.lock, flags);

	/*
	 * Disable break condition and FIFOs
	 */
	serial_out(up, UART_LCR, serial_in(up, UART_LCR) & ~UART_LCR_SBC);
	serial_out(up, UART_FCR, UART_FCR_ENABLE_FIFO |
				  UART_FCR_CLEAR_RCVR |
				  UART_FCR_CLEAR_XMIT);
	serial_out(up, UART_FCR, 0);
}

#define BOTH_EMPTY (UART_LSR_TEMT | UART_LSR_THRE)

/*
 *	Wait for transmitter & holding register to empty
 */
static inline void wait_for_xmitr(struct uart_fmsh_port *up)
{
	unsigned int status, tmout = 10000;

	/* Wait up to 10ms for the character(s) to be sent. */
	do {
		status = serial_in(up, UART_LSR);

		if (status & UART_LSR_BI)
			up->lsr_break_flag = UART_LSR_BI;

		if (--tmout == 0)
			break;
		udelay(1);
	} while ((status & BOTH_EMPTY) != BOTH_EMPTY);

	/* Wait up to 1s for flow control if necessary */
	if (up->port.flags & UPF_CONS_FLOW) {
		tmout = 1000000;
		while (--tmout &&
		       ((serial_in(up, UART_MSR) & UART_MSR_CTS) == 0))
			udelay(1);
	}
}

static void force_idle(struct uart_fmsh_port *up)
{
	unsigned int fcr = serial_in(up, UART_FCR);

	serial_out(up, UART_FCR, UART_FCR_ENABLE_FIFO);
	serial_out(up, UART_FCR, UART_FCR_ENABLE_FIFO |
			   UART_FCR_CLEAR_RCVR | UART_FCR_CLEAR_XMIT);
	serial_out(up, UART_FCR, 0);
	serial_out(up, UART_FCR, fcr);
	(void)serial_in(up, UART_RX);
}

static void set_lcr(struct uart_fmsh_port *up, int value)
{
	int tries = 1000;

	/* Make sure LCR write wasn't ignored */
	while (tries--) {
		unsigned int lcr = serial_in(up, UART_LCR);

		if ((value & ~UART_LCR_SPAR) == (lcr & ~UART_LCR_SPAR))
			return;

		force_idle(up);
		serial_out(up, UART_LCR, value);
	}
	/*
	 * FIXME: this deadlocks if port->lock is already held
	 */
	dev_dbg(up->port.dev, "Couldn't set LCR to %d\n", value);
}

static void
fuartps_set_termios(struct uart_port *port, struct ktermios *termios,
		       struct ktermios *old)
{
	struct uart_fmsh_port *up = (struct uart_fmsh_port *)port;
	unsigned char cval, fcr = 0;
	unsigned long flags;
	long quot_f;
	unsigned int baud, quot;

	switch (termios->c_cflag & CSIZE) {
	case CS5:
		cval = UART_LCR_WLEN5;
		break;
	case CS6:
		cval = UART_LCR_WLEN6;
		break;
	case CS7:
		cval = UART_LCR_WLEN7;
		break;
	default:
	case CS8:
		cval = UART_LCR_WLEN8;
		break;
	}

	if (termios->c_cflag & CSTOPB)
		cval |= UART_LCR_STOP;
	if (termios->c_cflag & PARENB)
		cval |= UART_LCR_PARITY;
	if (!(termios->c_cflag & PARODD))
		cval |= UART_LCR_EPAR;

	/*
	 * Ask the core to calculate the divisor for us.
	 */
	baud = uart_get_baud_rate(port, termios, old, 0, port->uartclk/16);
	quot_f = (port->uartclk) / (16 * (baud / 100));
	quot = quot_f / 100;
	quot_f = (quot_f % 100) * 16 / 100;

	dev_dbg(port->dev, "division decimal:%d, fractional:%ld\n",quot, quot_f);

	if ((up->port.uartclk / quot) < (2400 * 16))
		fcr = UART_FCR_ENABLE_FIFO | UART_FCR_PXAR1;
	else if ((up->port.uartclk / quot) < (230400 * 16))
		fcr = UART_FCR_ENABLE_FIFO | UART_FCR_PXAR8;
	else
		fcr = UART_FCR_ENABLE_FIFO | UART_FCR_PXAR32;

	/*
	 * Ok, we're now changing the port state.  Do it with
	 * interrupts disabled.
	 */
	spin_lock_irqsave(&up->port.lock, flags);

	/*
	 * Ensure the port will be enabled.
	 * This is required especially for serial console.
	 */
	up->ier |= UART_IER_UUE;

	/*
	 * Update the per-port timeout.
	 */
	uart_update_timeout(port, termios->c_cflag, baud);

	up->port.read_status_mask = UART_LSR_OE | UART_LSR_THRE | UART_LSR_DR;
	if (termios->c_iflag & INPCK)
		up->port.read_status_mask |= UART_LSR_FE | UART_LSR_PE;
	if (termios->c_iflag & (IGNBRK | BRKINT | PARMRK))
		up->port.read_status_mask |= UART_LSR_BI;

	/*
	 * Characters to ignore
	 */
	up->port.ignore_status_mask = 0;
	if (termios->c_iflag & IGNPAR)
		up->port.ignore_status_mask |= UART_LSR_PE | UART_LSR_FE;
	if (termios->c_iflag & IGNBRK) {
		up->port.ignore_status_mask |= UART_LSR_BI;
		/*
		 * If we're ignoring parity and break indicators,
		 * ignore overruns too (for real raw support).
		 */
		if (termios->c_iflag & IGNPAR)
			up->port.ignore_status_mask |= UART_LSR_OE;
	}

	/*
	 * ignore all characters if CREAD is not set
	 */
	if ((termios->c_cflag & CREAD) == 0)
		up->port.ignore_status_mask |= UART_LSR_DR;

	/*
	 * CTS flow control flag and modem status interrupts
	 */
	up->ier &= ~UART_IER_MSI;
	if (UART_ENABLE_MS(&up->port, termios->c_cflag))
		up->ier |= UART_IER_MSI;

	serial_out(up, UART_IER, up->ier);

	if (termios->c_cflag & CRTSCTS)
		up->mcr |= UART_MCR_AFE;
	else
		up->mcr &= ~UART_MCR_AFE;

	set_lcr(up, cval | UART_LCR_DLAB);	/* set DLAB */

	serial_out(up, UART_DLL, quot & 0xff);		/* LS of divisor */
	serial_out(up, UART_DLM, quot >> 8);		/* MS of divisor */
	serial_out(up, UART_DLF, quot_f & 0xff);	/* fractional part of divisor */

	set_lcr(up, cval);			/* reset DLAB */
	up->lcr = cval;					/* Save LCR */

	fuartps_set_mctrl(&up->port, up->port.mctrl);
	serial_out(up, UART_FCR, fcr);
	spin_unlock_irqrestore(&up->port.lock, flags);
}

static void
fuartps_pm(struct uart_port *port, unsigned int state,
	      unsigned int oldstate)
{
	struct uart_fmsh_port *up = (struct uart_fmsh_port *)port;

	if (!state)
		clk_prepare_enable(up->clk);
	else
		clk_disable_unprepare(up->clk);
}

static void fuartps_release_port(struct uart_port *port)
{
}

static int fuartps_request_port(struct uart_port *port)
{
	return 0;
}

static void fuartps_config_port(struct uart_port *port, int flags)
{
	struct uart_fmsh_port *up = (struct uart_fmsh_port *)port;
	up->port.type = PORT_FUARTPS;
}

static int
fuartps_verify_port(struct uart_port *port, struct serial_struct *ser)
{
	/* we don't want the core code to modify any port params */
	return -EINVAL;
}

static const char *
fuartps_type(struct uart_port *port)
{
	struct uart_fmsh_port *up = (struct uart_fmsh_port *)port;
	return up->name;
}

static struct uart_fmsh_port *fuartps_ports[2];
static struct uart_driver fuartps_reg;

#ifdef CONFIG_SERIAL_FMQL_PS_UART_CONSOLE

static void fuartps_console_putchar(struct uart_port *port, int ch)
{
	struct uart_fmsh_port *up = (struct uart_fmsh_port *)port;

	wait_for_xmitr(up);
	serial_out(up, UART_TX, ch);
}

/*
 * Print a string to the serial port trying not to disturb
 * any possible real use of the port...
 *
 *	The console_lock must be held when we get here.
 */
static void
fuartps_console_write(struct console *co, const char *s, unsigned int count)
{
	struct uart_fmsh_port *up = fuartps_ports[co->index];
	unsigned int ier;
	unsigned long flags;
	int locked = 1;

	clk_enable(up->clk);
	local_irq_save(flags);
	if (up->port.sysrq)
		locked = 0;
	else if (oops_in_progress)
		locked = spin_trylock(&up->port.lock);
	else
		spin_lock(&up->port.lock);

	/*
	 *	First save the IER then disable the interrupts
	 */
	ier = serial_in(up, UART_IER);
	serial_out(up, UART_IER, UART_IER_UUE);

	uart_console_write(&up->port, s, count, fuartps_console_putchar);

	/*
	 *	Finally, wait for transmitter to become empty
	 *	and restore the IER
	 */
	wait_for_xmitr(up);
	serial_out(up, UART_IER, ier);

	if (locked)
		spin_unlock(&up->port.lock);
	local_irq_restore(flags);
	clk_disable(up->clk);
}

#ifdef CONFIG_CONSOLE_POLL
/*
 * Console polling routines for writing and reading from the uart while
 * in an interrupt or debug context.
 */

static int fuartps_get_poll_char(struct uart_port *port)
{
	struct uart_fmsh_port *up = (struct uart_fmsh_port *)port;
	unsigned char lsr = serial_in(up, UART_LSR);

	while (!(lsr & UART_LSR_DR))
		lsr = serial_in(up, UART_LSR);

	return serial_in(up, UART_RX);
}


static void fuartps_put_poll_char(struct uart_port *port,
			 unsigned char c)
{
	unsigned int ier;
	struct uart_fmsh_port *up = (struct uart_fmsh_port *)port;

	/*
	 *	First save the IER then disable the interrupts
	 */
	ier = serial_in(up, UART_IER);
	serial_out(up, UART_IER, UART_IER_UUE);

	wait_for_xmitr(up);
	/*
	 *	Send the character out.
	 *	If a LF, also do CR...
	 */
	serial_out(up, UART_TX, c);
	if (c == 10) {
		wait_for_xmitr(up);
		serial_out(up, UART_TX, 13);
	}

	/*
	 *	Finally, wait for transmitter to become empty
	 *	and restore the IER
	 */
	wait_for_xmitr(up);
	serial_out(up, UART_IER, ier);
}

#endif /* CONFIG_CONSOLE_POLL */

static int __init
fuartps_console_setup(struct console *co, char *options)
{
	struct uart_fmsh_port *up;
	int baud = 115200;
	int bits = 8;
	int parity = 'n';
	int flow = 'n';

	if (co->index == -1 || co->index >= fuartps_reg.nr)
		co->index = 0;
	up = fuartps_ports[co->index];
	if (!up)
		return -ENODEV;

	if (options)
		uart_parse_options(options, &baud, &parity, &bits, &flow);

	return uart_set_options(&up->port, co, baud, parity, bits, flow);
}

static struct console fuartps_console = {
	.name		= FUARTPS_TTY_NAME,
	.write		= fuartps_console_write,
	.device		= uart_console_device,
	.setup		= fuartps_console_setup,
	.flags		= CON_PRINTBUFFER,
	.index		= -1,
	.data		= &fuartps_reg,
};

#define FMSH_CONSOLE	&fuartps_console
#else
#define FMSH_CONSOLE	NULL
#endif

static struct uart_ops fuartps_ops = {
	.tx_empty	= fuartps_tx_empty,
	.set_mctrl	= fuartps_set_mctrl,
	.get_mctrl	= fuartps_get_mctrl,
	.stop_tx	= fuartps_stop_tx,
	.start_tx	= fuartps_start_tx,
	.stop_rx	= fuartps_stop_rx,
	.enable_ms	= fuartps_enable_ms,
	.break_ctl	= fuartps_break_ctl,
	.startup	= fuartps_startup,
	.shutdown	= fuartps_shutdown,
	.set_termios	= fuartps_set_termios,
	.pm		= fuartps_pm,
	.type		= fuartps_type,
	.release_port	= fuartps_release_port,
	.request_port	= fuartps_request_port,
	.config_port	= fuartps_config_port,
	.verify_port	= fuartps_verify_port,
#ifdef CONFIG_CONSOLE_POLL
	.poll_get_char = fuartps_get_poll_char,
	.poll_put_char = fuartps_put_poll_char,
#endif
};

static struct uart_driver fuartps_reg = {
	.owner		= THIS_MODULE,
	.driver_name	= FUARTPS_NAME,
	.dev_name	= FUARTPS_TTY_NAME,
#if 0
	.major		= TTY_MAJOR,
	.minor		= 64,
#endif
	.nr		= FUARTPS_NR_PORTS,
	.cons		= FMSH_CONSOLE,
};

#ifdef CONFIG_PM
static int fuartps_suspend(struct device *dev)
{
        struct uart_fmsh_port *sport = dev_get_drvdata(dev);

        if (sport)
                uart_suspend_port(&fuartps_reg, &sport->port);

        return 0;
}

static int fuartps_resume(struct device *dev)
{
        struct uart_fmsh_port *sport = dev_get_drvdata(dev);

        if (sport)
                uart_resume_port(&fuartps_reg, &sport->port);

        return 0;
}

static const struct dev_pm_ops fuartps_pm_ops = {
	.suspend	= fuartps_suspend,
	.resume		= fuartps_resume,
};
#endif

static struct of_device_id fuartps_dt_ids[] = {
	{ .compatible = "fmsh,fuartps", },
	{}
};
MODULE_DEVICE_TABLE(of, fuartps_dt_ids);

static int fuartps_probe_dt(struct platform_device *pdev,
			       struct uart_fmsh_port *sport)
{
	struct device_node *np = pdev->dev.of_node;
	int ret;

	if (!np)
		return 1;

	ret = of_alias_get_id(np, "serial");
	if (ret < 0) {
		dev_err(&pdev->dev, "failed to get alias id, errno %d\n", ret);
		return ret;
	}
	sport->port.line = ret;
	return 0;
}

static int fuartps_probe(struct platform_device *dev)
{
	struct uart_fmsh_port *sport;
	struct resource *mmres, *irqres, *irqres1;
	int ret;

	mmres = platform_get_resource(dev, IORESOURCE_MEM, 0);
	irqres = platform_get_resource(dev, IORESOURCE_IRQ, 0);
	if (!mmres || !irqres)
		return -ENODEV;

	irqres1 = platform_get_resource(dev, IORESOURCE_IRQ, 1);
	if (!irqres1)
		return -ENODEV;

	sport = kzalloc(sizeof(struct uart_fmsh_port), GFP_KERNEL);
	if (!sport)
		return -ENOMEM;

	sport->clk = devm_clk_get(&dev->dev, "uart_ref");
	if (IS_ERR(sport->clk)) {
		ret = PTR_ERR(sport->clk);
		goto err_free;
	}

	ret = clk_prepare(sport->clk);
	if (ret) {
		goto err_free;
	}

	sport->pclk = devm_clk_get(&dev->dev, "pclk");
	if (IS_ERR(sport->pclk)) {
		ret = PTR_ERR(sport->pclk);
		goto err_pclk;
	}

	ret = clk_prepare_enable(sport->pclk);
	if (ret) {
		goto err_pclk;
	}

	sport->port.type = PORT_FUARTPS;
	sport->port.iotype = UPIO_MEM;
	sport->port.mapbase = mmres->start;
	sport->port.irq = irqres->start;
	sport->port.fifosize = FUARTPS_FIFO_SIZE;
	sport->port.ops = &fuartps_ops;
	sport->port.dev = &dev->dev;
	sport->port.flags = UPF_IOREMAP | UPF_BOOT_AUTOCONF;
	sport->port.uartclk = clk_get_rate(sport->clk);

	ret = fuartps_probe_dt(dev, sport);
	if (ret > 0)
		sport->port.line = dev->id;
	else if (ret < 0)
		goto err_clk;
	snprintf(sport->name, FMSH_NAME_LEN - 1, "UART%d", sport->port.line + 1);

	sport->port.membase = ioremap(sport->port.mapbase, resource_size(mmres));
	dev_dbg(&dev->dev, "base phy addr:0x%x, virtual addr:%p, refclk:%dHz\n",
				sport->port.mapbase, sport->port.membase, sport->port.uartclk);
	if (!sport->port.membase) {
		ret = -ENOMEM;
		goto err_clk;
	}

	fuartps_ports[sport->port.line] = sport;

	uart_add_one_port(&fuartps_reg, &sport->port);
	platform_set_drvdata(dev, sport);

	printk("Debug %s %d get irq1 %lld\n", __func__, __LINE__, irqres1->start);
	s_irq1 = irqres1->start;
	/*
	ret = request_irq(irqres1->start, fuartps_irq1, IRQF_SHARED, NULL, up);
	if (ret)
		goto err_clk;
		*/

	return 0;

 err_clk:
	clk_unprepare(sport->clk);
 err_pclk:
	clk_disable_unprepare(sport->pclk);
 err_free:
	kfree(sport);
	return ret;
}

static int fuartps_remove(struct platform_device *dev)
{
	struct uart_fmsh_port *sport = platform_get_drvdata(dev);

	uart_remove_one_port(&fuartps_reg, &sport->port);

	clk_unprepare(sport->clk);
	clk_put(sport->clk);
	kfree(sport);

	return 0;
}

static struct platform_driver fuartps_driver = {
	.probe          = fuartps_probe,
	.remove         = fuartps_remove,

	.driver		= {
		.name	= FUARTPS_NAME,
		.owner	= THIS_MODULE,
#ifdef CONFIG_PM
		.pm	= &fuartps_pm_ops,
#endif
		.of_match_table = fuartps_dt_ids,
	},
};

static int __init fuartps_init(void)
{
	int ret;

	ret = uart_register_driver(&fuartps_reg);
	if (ret != 0)
		return ret;

	ret = platform_driver_register(&fuartps_driver);
	if (ret != 0)
		uart_unregister_driver(&fuartps_reg);

	return ret;
}

static void __exit fuartps_exit(void)
{
	platform_driver_unregister(&fuartps_driver);
	uart_unregister_driver(&fuartps_reg);
}

module_init(fuartps_init);
module_exit(fuartps_exit);

MODULE_LICENSE("GPL");
MODULE_ALIAS("platform:fmshPS-uart");
