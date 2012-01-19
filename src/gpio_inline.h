extern volatile unsigned long *gpio;
extern volatile unsigned long *mfpr;

static inline void gpio_output(int i, int dir)
{
	i = gpio_table[i];
	volatile unsigned long *gpdr;
	
	if (i < 96)
		gpdr = gpio + 0xC/4 + i/32;
	else
		gpdr = gpio + 0x10c/4 + (i - 96) / 32;
		
	i &= 31;
	
	if (dir)
		*gpdr |= 1<<i;
	else
		*gpdr &=~(1<<i);
}

static inline void gpio_set(int i, int val)
{
	i = gpio_table[i];
	volatile unsigned long *gpxr;
	
	if (i < 96)
		gpxr = gpio + 0x18/4 + i/32;
	else
		gpxr = gpio + 0x118/4 + (i - 96) / 32;
	
	if (!val)
		gpxr += 3; // GPSR -> GPCR
		
	i &= 31;
	
	*gpxr |= 1<<i;
}

static inline int gpio_get(int i)
{
	i = gpio_table[i];
	volatile unsigned long *gplr;
	
	if (i < 96)
		gplr = gpio + i/32;
	else
		gplr = gpio + 0x100/4 + (i - 96) / 32;
		
	i &= 31;
	
	return !!((*gplr) & (1<<i));
}

