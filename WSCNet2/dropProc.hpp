#pragma once

#include "dropFuncs.hpp"


void readImgNames(const std::string& img_path, std::vector<std::string>& img_names, const std::vector<std::string>& img_ext_vec);

int dropFindBright(const cv::Mat& src_gray, const Parameter& param, int dev, ScoreCircle& final_circles);

int dropFindDark(const cv::Mat& src_gray, const Parameter& param, ScoreCircle& final_circles);

void drawDropCircle(cv::Mat& src_img, int drop_x, int drop_y, int drop_r, int drop_label);