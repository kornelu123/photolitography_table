#pragma once

#include <opencv2/opencv.hpp>

#define WIDTH       640
#define HEIGHT      360

cv::Mat read_img(const char *fname, const uint8_t brightness);
int write_img(uint8_t *data, uint32_t size);
int blackout_screen(void);
cv::Mat moveRightToLeft(const cv::Mat& input, int nPixel);
int evm_reset(void);
int evm_off(void);
int evm_on(void);
int curtain_evm_off(void);
int curtain_evm_on(void);
