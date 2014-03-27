/* 2011-06-10: File added and changed by Sony Corporation */
#ifndef _A1026_H_
#define _A1026_H_

#define VERSION_NBX0200		0x02
#define VERSION_NBX0300		0x03
#define VERSION_PASSTHROUGH		0x04
#define VERSION_UNKNOWN		0xff

struct a1026_platform_data {
	struct i2c_client *client;
	int reset_pin;
	int wake_pin;
	struct clk *clock;
	char *clk;
        int fw_version;
        int is_awake;
};

int a1026_suspend_command(void);
int a1026_resume_command(void);

#endif /* _A1026_H_ */
