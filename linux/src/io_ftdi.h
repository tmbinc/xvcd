void io_close(void);
int io_init(int vendor, int product, const char* desc);
int io_scan(const unsigned char *TMS, const unsigned char *TDI, unsigned char *TDO, int bits);
