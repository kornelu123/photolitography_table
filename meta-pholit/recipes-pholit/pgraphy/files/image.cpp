#include <opencv2/opencv.hpp>
#include <opencv2/core.hpp>
#include <vector>
#include <lcdc_drv.h>

#include <sys/ioctl.h>
#include <stdio.h>
#include <unistd.h>

#include "image.hpp"

extern int fb_fd;
uint8_t blackout_buff[WIDTH*HEIGHT*3];

cv::Mat
read_img(const char *fname, const uint8_t brightness)
{
    cv::Mat src;
    src = cv::imread(fname, cv::IMREAD_COLOR);

    if(src.empty()) {
        fprintf(stderr, "cv::imread failed\n");
        exit(-1);
    }

    src.convertTo(src, CV_32F);
    src *= (float)brightness/(float)255; // 20% brighter
    src.convertTo(src, CV_8U);

    cv::Mat rgb_src(src.size(), src.type());
    int fromTo[] = {0, 2, 1, 0, 2, 1};
    cv::mixChannels(&src, 1, &rgb_src, 1, fromTo, 3);

    return rgb_src;
}

    int
write_img(uint8_t *data, uint32_t size)
{
    int ret = write(fb_fd, data, size);
    if (ret == -1) {
        perror("write:");
        exit(-1);
    }

    ret = lseek(fb_fd, 0, SEEK_SET);
    if (ret == -1) {
        perror("lseek:");
        exit(-1);
    }

    return 0;
}

    int 
evm_reset(void)
{
    return ioctl(fb_fd, FB_RESET, NULL);
}

    int 
evm_off(void)
{
    return ioctl(fb_fd, FB_OFF, NULL);
}

    int 
evm_on(void)
{
    return ioctl(fb_fd, FB_ON, NULL);
}
    int
curtain_evm_on(void)
{
    return ioctl(fb_fd, FB_BLACKOUT, NULL);
}

    int
curtain_evm_off(void)
{
    return ioctl(fb_fd, FB_RESTORE, NULL);
}

    int
blackout_screen(void)
{
    return write_img(blackout_buff, WIDTH*HEIGHT*3);
}

cv::Mat
moveRightToLeft(const cv::Mat& input, int nPixels) {
    if (nPixels <= 0 || nPixels >= input.cols) {
        return input.clone();
    }

    cv::Mat output(input.size(), input.type());

    cv::Mat rightPart = input(cv::Rect(input.cols - nPixels, 0, nPixels, input.rows));

    cv::Mat leftPart = input(cv::Rect(0, 0, input.cols - nPixels, input.rows));

    rightPart.copyTo(output(cv::Rect(0, 0, nPixels, input.rows)));
    leftPart.copyTo(output(cv::Rect(nPixels, 0, leftPart.cols, input.rows)));

    return output;
}
