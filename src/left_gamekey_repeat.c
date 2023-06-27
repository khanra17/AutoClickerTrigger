#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <linux/input.h>
#include <poll.h>
#include <pthread.h>
#include <dirent.h>
#include <string.h>

#define MAX_DEVICE_NAME_LEN 256

char trigger_dev[MAX_DEVICE_NAME_LEN];
volatile int counter = -1;
pthread_t thread;

char *find_trigger_device()
{
    DIR *dp;
    struct dirent *ep;
    char device_name[MAX_DEVICE_NAME_LEN];
    char command[MAX_DEVICE_NAME_LEN];
    static char trigger_dev[MAX_DEVICE_NAME_LEN];

    dp = opendir("/dev/input");
    if (dp != NULL)
    {
        while ((ep = readdir(dp)) != NULL)
        {
            if (strstr(ep->d_name, "event") != NULL)
            {
                sprintf(command, "getevent -i /dev/input/%s | grep -q \"xm_gamekey\"", ep->d_name);
                if (system(command) == 0)
                {
                    sprintf(device_name, "/dev/input/%s", ep->d_name);
                    strncpy(trigger_dev, device_name, MAX_DEVICE_NAME_LEN);
                    closedir(dp);
                    return trigger_dev;
                }
            }
        }
        (void)closedir(dp);
    }
    perror("Couldn't find the trigger device");
    exit(EXIT_FAILURE);
}

void send_key_event(int fevent, int type, int key_code, int value)
{
    struct input_event event;
    event.type = type;
    event.code = key_code;
    event.value = value;
    gettimeofday(&event.time, NULL);
    write(fevent, &event, sizeof(event));
}

void *loop_thread(void *arg)
{
    int file_event = open(trigger_dev, O_WRONLY);
    if (file_event < 0)
    {
        perror("Failed to open input device");
        exit(EXIT_FAILURE);
    }
    while (1)
    {
        if (counter > 0)
        {
            send_key_event(file_event, EV_KEY, KEY_F1, 0);
            send_key_event(file_event, EV_SYN, SYN_REPORT, 0);
            usleep(1);

            send_key_event(file_event, EV_KEY, KEY_F1, 3);
            send_key_event(file_event, EV_SYN, SYN_REPORT, 0);
            counter++;
        }
        usleep(50000);
    }
    return NULL;
}

int main(int argc, char **argv)
{
    strcpy(trigger_dev, find_trigger_device());
    printf("Trigger device: %s\n", trigger_dev);

    int file_event = open(trigger_dev, O_RDONLY);
    if (file_event < 0)
    {
        perror("open");
        exit(EXIT_FAILURE);
    }

    // Setup pollfd structure for input device
    struct pollfd pfd = {0};
    pfd.fd = file_event;
    pfd.events = POLLIN;

    pthread_create(&thread, NULL, loop_thread, NULL);

    while (1)
    {
        // Wait for input event using poll()
        int ret = poll(&pfd, 1, -1);
        if (ret < 0)
        {
            perror("poll");
            exit(EXIT_FAILURE);
        }
        if (pfd.revents & POLLIN)
        {
            // Read input event
            struct input_event event;
            if (read(file_event, &event, sizeof(event)) < 0)
            {
                perror("read");
                exit(EXIT_FAILURE);
            }
            if (event.type == EV_KEY && event.code == KEY_F1 && event.value == 1)
            {
                counter = 1;
            }
            else if (event.type == EV_KEY && event.code == KEY_F1 && event.value == 0)
            {
                counter--;
            }
            else if (event.type == EV_KEY && event.code == KEY_F4) // trigger close
            {
                counter = -1;
            }
        }
    }

    close(file_event);
    return 0;
}