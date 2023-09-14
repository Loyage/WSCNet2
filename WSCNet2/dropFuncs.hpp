#pragma once

#include <opencv2/opencv.hpp>
#include <vector>
#include <omp.h>//多线程

constexpr double pi = 3.1416;

typedef std::pair<cv::Point2f, float> CIRCLE;

class ScoreCircle
{
public:
	std::vector<CIRCLE> circles;
	std::vector<float> scores;
	ScoreCircle(std::vector<CIRCLE>c, std::vector<float>s);
	ScoreCircle();
	int size();
};

class Parameter
{
public:
	const int kernel_size;
	const float min_radius;
	const float max_radius;
	const float area_rate;
	const bool is_find_overlap;
	const bool parameter_adjust;
	const bool isVisualization;
	int wait_time;
	Parameter(int kernelSize, float minRadius, float maxRadius, float area_rate, bool is_find_overlap, bool parameterAdjust, bool visualization, int wait_time);
};

class GrayThreshold
{
public:
	int grayThd_low;
	int grayThd_high;
	int center_low;
	int center_middle;
	int center_high;

};

//双高斯方差和最小化模型
void minVarThd(const cv::Mat &src_gray, int &thd_minVar, std::pair<int, float> &leftGauss, std::pair<int, float>& rightGauss, bool delZero);

//得到两个灰度阈值分割图像
void minVarSegmentation(const cv::Mat &src_gray, GrayThreshold &output_gthd, const Parameter &param);

void extractContours(cv::Mat &bw_img, std::vector<std::vector<cv::Point>> &pCountour_all, const Parameter &param);

//搜索重叠液滴
void findOverlaps(const cv::Mat &src_gray, const GrayThreshold &input_gthd, const Parameter param, 
										cv::Mat & output_img, std::vector<CIRCLE> &overlapCircles);

//单液滴绘制
void drawSingleDrop(const cv::Mat &drops_without_dark, cv::Mat &output_img, ScoreCircle &sCircle, const Parameter &param, const GrayThreshold &gthd);

//连通域分割单液滴
void findSingleDrop(const cv::Mat &src_bw_img, ScoreCircle &scoreCircles, const Parameter &param, int dev=0);

//单液滴refine
void singleDropRefine(const cv::Mat &src_img, CIRCLE oneCircle, CIRCLE &refineCircle);

//搜索液滴
void findDroplet(cv::Mat &drops_without_dark, cv::Mat &mask, ScoreCircle &sCircle, cv::Mat &bw_drop_img, const Parameter &param, const GrayThreshold &gthd);

//密度收缩搜索液滴
void findDrops(const cv::Mat &dst_img, ScoreCircle &scoreCircles, const Parameter &param);

//NMS过滤液滴分割结果
void NMS(const ScoreCircle sCircles, ScoreCircle &outputScoreCircles, bool useArea=0);

//合并结果
void mergeDropResult(const cv::Mat &src_bw_img, ScoreCircle contected_drop_circles, ScoreCircle single_drop_circles, ScoreCircle &mergedCircles, const Parameter &param);

//计算两个圆相交的面积
float intersectionArea(CIRCLE circle_1, CIRCLE circle_2);

//明场液滴搜索
void findLightDrop(const cv::Mat &src_gray, std::vector<CIRCLE> &final_circles, const Parameter &param, const int dev);

//空洞填充
void fillHole(const cv::Mat src_bw, cv::Mat &dst_bw);

void extractContoursBright(const cv::Mat& src_gray, std::vector<std::vector<cv::Point>>& pCountour_all, int kernelSize, const double area_rate, const double min_area, const double max_area, bool parameter_adjust);