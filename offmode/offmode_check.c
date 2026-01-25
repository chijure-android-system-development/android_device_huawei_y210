#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include <cutils/properties.h>

int get_chargemode() {
    char buffer[1024], *p, *q;
    int len;
    int fd = open("/proc/app_info",O_RDONLY);
    int flag = 0;

    if (fd < 0)
        fprintf(stderr, "E: could not open /proc/app_info: %s\n", strerror(errno));
    do 
		len = read(fd,buffer,sizeof(buffer));
    while
		(len == -1 && errno == EINTR);

    if (len < 0) {
        fprintf(stderr, "E: could not read /proc/app_info: %s\n", strerror(errno));
        close(fd);
    }
    close(fd);
	char * charge;
	charge = strstr(buffer, "charge_flag");

    if ((int)charge[13] == 49) {	
		printf("I: Charge Flag: %c\n", charge[13]);
		flag = 1;
    }

	return flag;
}

int main(void){
    int flag;
    flag = get_chargemode();
    if (flag) {
		printf("I: Huawei offmode charging\n");
		property_set("ctl.start", "charge");
        property_set("ctl.stop", "recovery");
	} else {
        sleep(3); //Give cache and misc partitions time to come online
        property_set("ctl.start", "recovery");
    }
	return 0;
}
