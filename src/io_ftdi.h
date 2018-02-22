void io_close(void);
int io_init(int vendor, int product, unsigned long frequency, int verbosity);
int io_scan(const unsigned char *TMS, const unsigned char *TDI, unsigned char *TDO, int bits);
int io_set_period(unsigned int period);
