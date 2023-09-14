#pragma once

#include <opencv2/opencv.hpp>
#include <vector>
#include <omp.h>//���߳�

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

//˫��˹�������С��ģ��
void minVarThd(const cv::Mat &src_gray, int &thd_minVar, std::pair<int, float> &leftGauss, std::pair<int, float>& rightGauss, bool delZero);

//�õ������Ҷ���ֵ�ָ�ͼ��
void minVarSegmentation(const cv::Mat &src_gray, GrayThreshold &output_gthd, const Parameter &param);

void extractContours(cv::Mat &bw_img, std::vector<std::vector<cv::Point>> &pCountour_all, const Parameter &param);

//�����ص�Һ��
void findOverlaps(const cv::Mat &src_gray, const GrayThreshold &input_gthd, const Parameter param, 
										cv::Mat & output_img, std::vector<CIRCLE> &overlapCircles);

//��Һ�λ���
void drawSingleDrop(const cv::Mat &drops_without_dark, cv::Mat &output_img, ScoreCircle &sCircle, const Parameter &param, const GrayThreshold &gthd);

//��ͨ��ָҺ��
void findSingleDrop(const cv::Mat &src_bw_img, ScoreCircle &scoreCircles, const Parameter &param, int dev=0);

//��Һ��refine
void singleDropRefine(const cv::Mat &src_img, CIRCLE oneCircle, CIRCLE &refineCircle);

//����Һ��
void findDroplet(cv::Mat &drops_without_dark, cv::Mat &mask, ScoreCircle &sCircle, cv::Mat &bw_drop_img, const Parameter &param, const GrayThreshold &gthd);

//�ܶ���������Һ��
void findDrops(const cv::Mat &dst_img, ScoreCircle &scoreCircles, const Parameter &param);

//NMS����Һ�ηָ���
void NMS(const ScoreCircle sCircles, ScoreCircle &outputScoreCircles, bool useArea=0);

//�ϲ����
void mergeDropResult(const cv::Mat &src_bw_img, ScoreCircle contected_drop_circles, ScoreCircle single_drop_circles, ScoreCircle &mergedCircles, const Parameter &param);

//��������Բ�ཻ�����
float intersectionArea(CIRCLE circle_1, CIRCLE circle_2);

//����Һ������
void findLightDrop(const cv::Mat &src_gray, std::vector<CIRCLE> &final_circles, const Parameter &param, const int dev);

//�ն����
void fillHole(const cv::Mat src_bw, cv::Mat &dst_bw);

void extractContoursBright(const cv::Mat& src_gray, std::vector<std::vector<cv::Point>>& pCountour_all, int kernelSize, const double area_rate, const double min_area, const double max_area, bool parameter_adjust);