#ifndef FB_CONSOLE_H
#define FB_CONSOLE_H

int fb_init(void);
int fb_write_string(const char *data);
int fb_write_hex(uint32 val);

#endif /* !FB_CONSOLE_H */
