#pragma once

#include <opencv2/opencv.hpp>

#define WIDTH       640
#define HEIGHT      360

cv::Mat read_img(const char *fname, const uint8_t brightness);
int write_img(uint8_t *data, uint32_t size);
int blackout_screen(void);
cv::Mat moveRightToLeft(const cv::Mat& input, int nPixel);
