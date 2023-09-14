#include "dropFuncs.hpp"

#include <opencv2/imgproc/types_c.h>
#include <numeric>

using namespace cv;
using namespace std;

Parameter::Parameter(int kernelSize, float minRadius, float maxRadius, float area_rate, bool is_find_overlap, bool parameterAdjust,bool visualization,int waitTime) :
kernel_size(kernelSize), min_radius(minRadius), max_radius(maxRadius), area_rate(area_rate),is_find_overlap(is_find_overlap), parameter_adjust(parameterAdjust), isVisualization(visualization), wait_time(waitTime)
{
	;
}

ScoreCircle::ScoreCircle(vector<CIRCLE>c, vector<float>s)
{
	if (c.size() == s.size())
	{
		circles.assign(c.begin(), c.end());
		scores.assign(s.begin(), s.end());
	}
	else
	{
		cout << "size mismatch!" << endl;
	}

}

ScoreCircle::ScoreCircle()
{
	;
}

int ScoreCircle::size()
{
	return circles.size();
}

void findLightDrop(const Mat &src_gray, vector<CIRCLE> &final_circles, const Parameter &param, const int dev)
{
	int kernelSize = param.kernel_size;
	Mat src_gray_copy;
	src_gray.copyTo(src_gray_copy);
	int thd;
	pair<int, float> leftGauss;
	pair<int, float> rightGauss;
	minVarThd(src_gray_copy, thd, leftGauss, rightGauss, 1);

	Mat bw_img;
	threshold(src_gray_copy, bw_img, thd, 255, THRESH_BINARY);

	Mat element_dilate = getStructuringElement(MORPH_ELLIPSE, Size(kernelSize, kernelSize));
	Mat element_erode = getStructuringElement(MORPH_ELLIPSE, Size(kernelSize, kernelSize));

	dilate(bw_img, bw_img, element_dilate);//膨胀 
	erode(bw_img, bw_img, element_erode);  //腐蚀 

	if (param.parameter_adjust)
	{
		imshow("bw_img", bw_img);
		waitKey();
	}

	ScoreCircle tempCircles;
	findSingleDrop(bw_img, tempCircles, param, dev);

	for (int i = 0; i != tempCircles.circles.size(); i++)
	{
		final_circles.push_back(CIRCLE(tempCircles.circles[i].first, tempCircles.circles[i].second + dev));
	}
	//circleFilter(final_circles, final_circles);
}

//双高斯方差和最小化前景分割
void minVarThd(const Mat &src_gray, int &thd_minVar, pair<int,float> &leftGauss, pair<int,float>& rightGauss,  bool delZero)
{
	int channels[] = { 0 };
	const int histSize = 256;
	//每一维bin的变化范围
	float range[] = { 0, 256 };
	const float *histRanges = { range };

	//定义直方图，这里求的是直方图数据
	Mat hist;

	calcHist(&src_gray, 1, 0, Mat(), hist, 1, &histSize, &histRanges, true, false);

	vector<float> histNumber;
	for (int i = 0; i < 256; i++)
	{
		//取每个bin的数目
		histNumber.push_back(hist.at<float>(i, 0));
	}
	if (delZero)
		histNumber[0] = 0;
	vector<float> var(histNumber.size(), 0.0);
	vector<pair<int, float>> left_gauss_all(histNumber.size());
	vector<pair<int, float>> right_gauss_all(histNumber.size());

	for (int j = 0; j < histNumber.size(); j++)
	{
		vector<float> gray_1;
		vector<float> weight_1;
		for (int index_1 = 0; index_1 < j + 1; index_1++)
		{
			gray_1.push_back(index_1 + 1);
			weight_1.push_back(histNumber[index_1]);
		}

		vector<float> gray_2;
		vector<float> weight_2;
		for (int index_2 = j + 1; index_2 < histNumber.size(); index_2++)
		{
			gray_2.push_back(index_2 + 1);
			weight_2.push_back(histNumber[index_2]);
		}

		float sum_1 = accumulate(weight_1.begin(), weight_1.end(), 0);  //权重和
		float sum_2 = accumulate(weight_2.begin(), weight_2.end(), 0);

		if (sum_1 <= 1 || sum_2 <= 1)  //无效分割
		{
			left_gauss_all[j] = pair<int,float>(-1,-1);
			right_gauss_all[j] = pair<int, float>(-1, -1);
			var[j] = 10000;
			continue;
		}

		//计算第一个均值
		float center_1(0.0);
		for (int iter_1 = 0; iter_1 < gray_1.size(); iter_1++)
		{
			center_1 += gray_1[iter_1] * weight_1[iter_1];
		}
		center_1 = center_1 / sum_1;

		//计算第一个方差
		float var_1(0.0);
		for (int iter_1 = 0; iter_1 != gray_1.size(); iter_1++)
		{
			var_1 += (gray_1[iter_1] - center_1)*(gray_1[iter_1] - center_1)*weight_1[iter_1];
		}
		var_1 = var_1 / sum_1;

		//计算第2个均值
		float center_2(0.0);
		for (int iter_2 = 0; iter_2 < gray_2.size(); iter_2++)
		{
			center_2 += gray_2[iter_2] * weight_2[iter_2];
		}
		center_2 = center_2 / sum_2;

		//计算第2个方差
		float var_2(0.0);
		for (int iter_2 = 0; iter_2 < gray_2.size(); iter_2++)
		{
			var_2 += (gray_2[iter_2] - center_2)*(gray_2[iter_2] - center_2)*weight_2[iter_2];
		}
		var_2 = var_2 / sum_2;

		//两个分布的权重方差之和
		float sum_weight1 = accumulate(weight_1.begin(), weight_1.end(),0);
		float sum_weight2 = accumulate(weight_2.begin(), weight_2.end(),0);

		float lamda = 0.5;// sum_weight1 / (sum_weight1 + sum_weight2);
		var[j] = lamda*var_1 + (1 - lamda)*var_2;
		//var[j] = var_1 + var_2;
		left_gauss_all[j] = pair<int,float>((int)center_1,var_1);
		right_gauss_all[j] = pair<int, float>((int)center_2, var_2);

	}

	vector<pair<float, int>> var_index;
	for (int i = 0; i != var.size(); i++)
	{
		var_index.push_back(pair<float, int>(var[i], i));
	}

	sort(var_index.begin(), var_index.end());

	thd_minVar = var_index[0].second;
	leftGauss = left_gauss_all[var_index[0].second];
	rightGauss = right_gauss_all[var_index[0].second];
}

void minVarSegmentation(const Mat &src_gray, GrayThreshold &output_gthd,const Parameter &param)
{
	int grayThd_low, grayThd_high, center1, center2, center3;
	pair<int, float> leftGauss, rightGauss;
	float var1, var2, var3;
	Mat src_gray_copy, img1, img2;

	src_gray.copyTo(src_gray_copy);
	bitwise_not(src_gray_copy, src_gray_copy); //反色

	src_gray_copy.copyTo(img2);

	float sum_var(100000.0);//很大值

	while (true)
	{
		minVarThd(img2, grayThd_low, leftGauss, rightGauss, 1);
		threshold(src_gray_copy, img1, grayThd_low, 255, THRESH_TOZERO);//背景小于阈值为0，前景为正常灰度
		var1 = leftGauss.second;
		center1 = leftGauss.first;

		if (!param.is_find_overlap)
		{
			grayThd_high = 254;
			break;
		}
		//imshow("img0", img1);
		//waitKey();

		minVarThd(img1, grayThd_high, leftGauss, rightGauss, 1);
		threshold(src_gray_copy, img2, grayThd_high, 255, THRESH_TOZERO_INV);

		//imshow("img2", img2);
		//waitKey();

		var2 = leftGauss.second;
		var3 = rightGauss.second;

		center2 = leftGauss.first;
		center3 = rightGauss.first;

		if ((var1 + var2 + var3) < sum_var)
			sum_var = var1 + var2 + var3;
		else
			break;
	}

	Mat showImg1, showImg3;
	src_gray.copyTo(showImg1);
	bitwise_not(showImg1, showImg1);
	src_gray.copyTo(showImg3);
	if (1 == showImg3.channels())
		cvtColor(showImg3, showImg3, COLOR_GRAY2BGR);
	Vec3b colorTab[] =
	{
		Vec3b(255, 100, 100),
		Vec3b(0, 0, 255),
		Vec3b(0, 255, 0),
	};
	
	int count_low(0), count_middle(0), count_high(0);

	//并行加速测试
	//clock_t start, finish;
	//float duration;
	//start = clock();

//#pragma omp parallel for
	for (int i = 0; i < showImg1.rows; i++)
	{
		for (int j = 0; j < showImg1.cols; j++)
		{
			if (showImg1.at<uchar>(i, j) < grayThd_low + 1)
			{
				count_low++;
				showImg3.at<Vec3b>(i, j) = colorTab[0];
			}
			else if (showImg1.at<uchar>(i, j) > grayThd_high + 1)
			{
				count_high++;
				showImg3.at<Vec3b>(i, j) = colorTab[1];
			}
			else
			{
				count_middle++;
				showImg3.at<Vec3b>(i, j) = colorTab[2];
			}
		}
	}

	if (count_high > 2 * count_middle)
		grayThd_high = 254;


	output_gthd.grayThd_low = grayThd_low + 1;
	output_gthd.grayThd_high = grayThd_high + 1;
	output_gthd.center_low = center1 + 1;
	output_gthd.center_middle = center2 + 1;
	output_gthd.center_high = center3 + 1;

	if (param.isVisualization)
	{
		imshow("Segmantation Image", showImg3);
		waitKey(param.wait_time);
	}
}

//从液滴前景中寻找重叠的液滴
void findOverlaps(const Mat &src_gray, const GrayThreshold &input_gthd, const Parameter param, Mat & output_img, vector<CIRCLE> &overlapCircles)
{
	if (!param.is_find_overlap)
	{
		src_gray.copyTo(output_img);
		return;
	}
	int kernelSize = param.kernel_size;
	float min_radius = param.min_radius;
	float max_radius = param.max_radius;
	bool parameter_adjust = param.parameter_adjust;
	int grayThd_low = input_gthd.grayThd_low;
	int grayThd_high = input_gthd.grayThd_high;
	int center_low = input_gthd.center_low;//背景灰度中心
	int center_middle = input_gthd.center_middle;//液滴内部灰度中心
	int center_high = input_gthd.center_high;//边缘灰度中心

	Mat element_dilate = getStructuringElement(MORPH_ELLIPSE, Size(kernelSize, kernelSize));
	Mat element_erode = getStructuringElement(MORPH_ELLIPSE, Size(kernelSize, kernelSize));

	int thd1, thd2;
	Mat inverseGray;
	bitwise_not(src_gray, inverseGray);  //图像取反
	Mat bw_img;
	
	threshold(inverseGray, bw_img, grayThd_low, 255, THRESH_TOZERO);//二值化
	threshold(bw_img, bw_img, grayThd_high, 255, THRESH_BINARY);//二值化

	erode(bw_img, bw_img, element_erode);  //腐蚀 
	dilate(bw_img, bw_img, element_dilate); //膨胀 

	if (parameter_adjust)
	{
		imshow("bw_img", bw_img);
		waitKey();
	}


	ScoreCircle score_drop_circles;
	findDrops(bw_img, score_drop_circles, param);

	Mat drops_without_dark;
	src_gray.copyTo(drops_without_dark);
	drops_without_dark.copyTo(output_img);


	if (grayThd_high == 255)
		return;

	vector<CIRCLE> dark_drop_circles;
	dark_drop_circles.assign(score_drop_circles.circles.begin(), score_drop_circles.circles.end());

	for (int i = 0; i != dark_drop_circles.size(); i++)
	{
		Point2f center(dark_drop_circles[i].first);
		float radius = dark_drop_circles[i].second+1;     //半径加1

		overlapCircles.push_back(CIRCLE(center,radius));

		//circle(drops_result, center, radius, Scalar(0,0,255), 1);
		Rect rect((int)(center.x - radius), (int)(center.y - radius), 2 * radius, 2 * radius);
		rect = rect&Rect(0, 0, src_gray.cols - 1, src_gray.rows - 1);
		
		Mat roi(drops_without_dark, rect);
		Mat roi_in;
		roi.copyTo(roi_in); // 用于处理内部

		Mat mask_out = Mat::ones(roi.size(), roi.type());  //获得外部的mask
		circle(mask_out, Point((int)radius, (int)radius), radius, 0, -1);

		roi = roi.mul(mask_out); //做一个内部为0的roi

		bitwise_not(roi_in, roi_in);//内部反色

		Mat mask_in = Mat::zeros(roi.size(), roi.type());  //获得内部的mask
		circle(mask_in, Point((int)radius, (int)radius), radius, 1, -1); 

		Mat mask_center = Mat::zeros(roi.size(), roi.type());  //获得中心的mask
		circle(mask_center, Point((int)radius, (int)radius), radius/4, 1, -1); //中心区域mask

		double meanGray;
		Scalar mean;  //均值
		Scalar stddev;  //标准差
		meanStdDev(roi_in, mean, stddev, mask_center);//寻找中心平均灰度值
		meanGray = mean.val[0];

		double _factor = meanGray / center_middle;  //中心平均灰度值与背景灰度中心之比（中心平均灰度为背景）

		roi_in = roi_in.mul(1 / _factor); //内部灰度放缩

		bitwise_not(roi_in, roi_in); //内部反色恢复
		roi_in = roi_in.mul(mask_in);

		add(roi, roi_in, roi);//roi修复
		blur(roi, roi, Size(3, 3));
		circle(roi, Point((int)radius, (int)radius), (int)(radius / 4+0.5), 256-center_low, -1); //中心区域置为背景灰度

		//imshow("roi2", roi);
		//waitKey();
	}

	drops_without_dark.copyTo(output_img);

	if (param.isVisualization)
	{
		if (dark_drop_circles.empty())
			return;
		Mat showImg;
		src_gray.copyTo(showImg);
		if (1 == showImg.channels())
			cvtColor(showImg, showImg, COLOR_GRAY2BGR);
		for (int i = 0; i != dark_drop_circles.size(); i++)
		{
			circle(showImg, dark_drop_circles[i].first, dark_drop_circles[i].second, Scalar(0, 0, 255), 2);
		}
		imshow("Overlapping Droplets", showImg);
		//imshow("Droplets without Overlaps", drops_without_dark);
		//waitKey();
		waitKey(param.wait_time);
	}

}

void drawSingleDrop(const Mat &drops_without_dark, Mat &output_img, ScoreCircle &sCircle, const Parameter &param, const GrayThreshold &gthd)
{
	int kernelSize = param.kernel_size;

	Mat element_dilate = getStructuringElement(MORPH_ELLIPSE, Size(kernelSize, kernelSize));
	Mat element_erode = getStructuringElement(MORPH_ELLIPSE, Size(kernelSize, kernelSize));

	Mat src_img;
	drops_without_dark.copyTo(src_img);
	bitwise_not(src_img, src_img);
	Mat bw_img;
	threshold(src_img, bw_img, gthd.grayThd_low, 255, THRESH_TOZERO);
	threshold(bw_img, bw_img, gthd.grayThd_high, 255, THRESH_TOZERO_INV);
	threshold(bw_img, bw_img, 1, 255, THRESH_BINARY);

	erode(bw_img, bw_img, element_erode);  //腐蚀 
	dilate(bw_img, bw_img, element_dilate);//膨胀 
	
	bw_img.copyTo(output_img);

	findSingleDrop(bw_img, sCircle, param);

	output_img = Mat::zeros(bw_img.size(), bw_img.type()); //输出图像初始化为全0;
	Mat result_img;
	drops_without_dark.copyTo(result_img);
	cvtColor(result_img, result_img, COLOR_GRAY2BGR);


	for (int i = 0; i != sCircle.circles.size(); i++)
	{
		circle(output_img, sCircle.circles[i].first, sCircle.circles[i].second, 255, -1);
		circle(result_img, sCircle.circles[i].first, sCircle.circles[i].second, Scalar(0, 200, 200), -1);
	}
	if (param.parameter_adjust)
	{
		imshow("Single Droplet Image", result_img);
		waitKey();
	}

}


void findSingleDrop(const Mat &src_bw_img, ScoreCircle &scoreCircles, const Parameter &param, int dev)
{
	Mat bw_img, bw_img_copy;
	src_bw_img.copyTo(bw_img);
	src_bw_img.copyTo(bw_img_copy);
	int min_radius = param.min_radius;
	int max_radius = param.max_radius;

	const float min_area = min_radius*min_radius*pi;
	const float max_area = max_radius*max_radius*pi;
	float area_rate = param.area_rate;

	vector<vector<Point>> pCountour_all;
	extractContours(bw_img, pCountour_all, param);

	for (int i = 0; i < pCountour_all.size(); i++)
	{

		Point2f center;
		float radius(0);
		//轮廓外接圆半径和圆心
		minEnclosingCircle(Mat(pCountour_all[i]), center, radius);
		float area_whole_circle = pi*radius*radius;  //在边界的非全圆，面积计算失效

		//计算外接圆的外接正方形roi
		Rect rect((int)(center.x - radius + 0.5), (int)(center.y - radius + 0.5), (int)(2 * radius + 0.5), (int)(2 * radius + 0.5));
		Rect roi_rect = rect&Rect(0, 0, bw_img.size().width - 1, bw_img.size().height - 1);

		//内接圆内全白的正方形mask
		Mat mask_img = Mat::zeros(bw_img.size(), bw_img.type());// 全0图
		circle(mask_img, center, radius, 255, -1, CV_8U);
		Mat roi_circle(mask_img, roi_rect);
		float area_circle = countNonZero(roi_circle);  //mask面积，即在不同情况下圆mask的面积

		if (area_circle < min_area)//面积过小者丢弃
			continue;
		if (area_circle / area_whole_circle < 0.5) //边界液滴拟合圆不满半圆者丢弃
			continue;

		//circle refine
		Mat roi_src(bw_img_copy, roi_rect);
		Mat roi_src_circle;
		roi_src.copyTo(roi_src_circle, roi_circle);
		//imshow("roi_src_circle", roi_src_circle);
		//waitKey();

		//circle_dst白色点个数
		int area_countour = contourArea(pCountour_all[i]);//countNonZero(roi_src_circle);
		if (area_countour / area_circle > area_rate &&area_whole_circle < max_area)//计算轮廓区域面积与圆mask面积之比
		{

			CIRCLE refineCircle;
			float rate_temp(area_circle / area_whole_circle);
			if (rate_temp > 0.7)
			{
				singleDropRefine(roi_src_circle, CIRCLE(center - Point2f(roi_rect.x, roi_rect.y), radius), refineCircle);
				refineCircle.first = refineCircle.first + Point2f(roi_rect.x, roi_rect.y);
			}
			else
			{
				refineCircle.first = center;
				refineCircle.second = radius;
			}

			if (refineCircle.second < param.min_radius)
				continue;

			scoreCircles.circles.push_back(CIRCLE(refineCircle.first, refineCircle.second));
			//
			Rect rect((int)(refineCircle.first.x - refineCircle.second + 0.5), (int)(refineCircle.first.y - refineCircle.second + 0.5), (int)(2 * refineCircle.second + 0.5), (int)(2 * refineCircle.second + 0.5));
			Rect roi_rect = rect&Rect(0, 0, bw_img.size().width - 1, bw_img.size().height - 1);
			Mat square_roi(src_bw_img, roi_rect);
			float m_square = countNonZero(square_roi);
			float single_score = area_countour / m_square;

			float score = rate_temp * single_score*area_countour / area_circle;
			score = score < 1.0 ? score : 1.0;

			scoreCircles.scores.push_back(score);
		}

	}
}

void singleDropRefine(const Mat &src_img, CIRCLE oneCircle, CIRCLE &refineCircle)
{

	float old_intensity = 2.0;
	float new_intensity = 1.0;

	refineCircle.first = oneCircle.first;
	refineCircle.second = oneCircle.second;

	Mat new_src;
	src_img.copyTo(new_src);
	int count(0);
	while (new_intensity < old_intensity)
	{
		old_intensity = new_intensity;

		//计算区域重心
		Moments moment = moments(new_src, false);
		float mass_x(0), mass_y(0);
		if (moment.m00 != 0)//除数不能为0
		{
			mass_x = cvRound(moment.m10 / moment.m00);//计算重心横坐标
			mass_y = cvRound(moment.m01 / moment.m00);//计算重心纵坐标
		}
		Point2f new_center;
		new_center.x = mass_x;
		new_center.y = mass_y;

		float new_radius = sqrt(moment.m00 /255/ pi);

		refineCircle.first = new_center;
		refineCircle.second = new_radius;

		Mat new_mask = Mat::zeros(src_img.size(), src_img.type());
		circle(new_mask, refineCircle.first, refineCircle.second, 1, -1);

		src_img.copyTo(new_src, new_mask);

		new_intensity = (float)countNonZero(new_src) / (float)countNonZero(new_mask);

		/***display***/
		if (false)
		{
			Mat display_circle;
			src_img.copyTo(display_circle);
			cvtColor(display_circle, display_circle, COLOR_GRAY2BGR);
			circle(display_circle, new_center, new_radius, Scalar(0, 0, 255), 1);
			circle(display_circle, new_center, 2, Scalar(0, 0, 255), -1);
			

			string center_name = "circle_dst_for_center" + to_string(count++);
			namedWindow(center_name);
			imshow(center_name, display_circle);
			waitKey();
		}
	}

}

//得到分割后的图像和单连通域
void extractContours(Mat &bw_img, vector<vector<Point>> &pCountour_all, const Parameter &param)
{
	Mat bw_copy;
	bw_img.copyTo(bw_copy);

	vector<vector<Point>> temp_countour;
	findContours(bw_copy, temp_countour, CV_RETR_LIST, CV_CHAIN_APPROX_SIMPLE, Point(0, 0));

	//防止出现内部大部分都是0（孔洞很大）的轮廓

	for (int i = 0; i < temp_countour.size(); i++)
	{
		Mat bw_copy2;
		bw_img.copyTo(bw_copy2);

		//轮廓rect
		Rect roi_rect = boundingRect(temp_countour[i])&Rect(0, 0, bw_img.size().width - 1, bw_img.size().height - 1);

		//轮廓内部填充白色的roi
		Mat zeros_img = Mat::zeros(bw_img.size(), bw_img.type());
		drawContours(zeros_img, temp_countour, i, 255, -1);
		Mat roi_countour = zeros_img(roi_rect);

		//二值图roi
		const Mat roi_src(bw_copy2, roi_rect);

		Mat countour_src;
		multiply(roi_countour, roi_src, countour_src);

		//轮廓内部白色点数
		int nonzeros = countNonZero(countour_src);

		//轮廓内部白色点数与轮廓区域面积占比，避免出现黑色填充的轮廓或者白色圆环
		if ((double)nonzeros / contourArea(temp_countour[i]) > 0.5)
		{
			pCountour_all.push_back(temp_countour[i]);
		}
	}

}

//搜索液滴
void findDroplet(Mat &drops_without_dark, Mat &mask, ScoreCircle	&sCircle, Mat &bw_drop_img, const Parameter &param, const GrayThreshold &gthd)
{
	int kernel_size = param.kernel_size;
	int min_radius = param.min_radius;
	int max_radius = param.max_radius;
	float area_rate = param.area_rate;
	bool parameter_adjust = param.parameter_adjust;

	const float min_area = min_radius*min_radius*pi;
	const float max_area = max_radius*max_radius*pi;

	Mat element_dilate = getStructuringElement(MORPH_ELLIPSE, Size(kernel_size, kernel_size));
	Mat element_erode = getStructuringElement(MORPH_ELLIPSE, Size(kernel_size, kernel_size));


	if (3 == drops_without_dark.channels())
		cvtColor(drops_without_dark, drops_without_dark, COLOR_RGB2GRAY);

	cout << "Processing the images of inseparable droplets..." << endl;


	Mat src_img, drop_mask;
	drops_without_dark.copyTo(src_img);
	bitwise_not(src_img, src_img);

	int thd = (gthd.grayThd_low + gthd.center_low) / 2;
	threshold(src_img, drop_mask, gthd.grayThd_low, 255, THRESH_BINARY);
 
	erode(drop_mask, drop_mask, element_erode);  //腐蚀 ,只进行腐蚀，则半径增加1
	//dilate(drop_mask, drop_mask, element_dilate);//膨胀

	add(drop_mask, mask, drop_mask);
	//erode(drop_mask, drop_mask, element_erode);  //腐蚀 

	drop_mask.copyTo(bw_drop_img);

	if (param.parameter_adjust)
	{
		//namedWindow("mask_dst");
		//moveWindow("mask_dst", 3400, 0);
		imshow("bw_drop_img", bw_drop_img);
		waitKey(param.wait_time);
	}

	ScoreCircle tempCircles;
	findDrops(drop_mask, tempCircles, param);

	Mat drops_result;
	drops_without_dark.copyTo(drops_result);
	cvtColor(drops_result, drops_result, COLOR_GRAY2RGB);
	for (int i = 0; i != tempCircles.circles.size(); i++)
	{
		sCircle.circles.push_back(CIRCLE(tempCircles.circles[i].first, tempCircles.circles[i].second + 1));
		sCircle.scores.push_back(tempCircles.scores[i]);
		circle(drops_result, tempCircles.circles[i].first, tempCircles.circles[i].second + 1, Scalar(0, 0, 255), 1);
	}
	if (param.parameter_adjust)
	{
		imshow("drops_result", drops_result);
		waitKey();
	}


}


//密度收缩法：从前景中搜索圆形前景
void findDrops(const Mat &dst_img, ScoreCircle &scoreCircles, const Parameter &param)
{
	float min_radius = param.min_radius;
	float start_radius = param.max_radius;
	bool isDisplay = param.parameter_adjust;

	Mat mask_dst;
	dst_img.copyTo(mask_dst);

	if (0 && isDisplay)
	{
		//namedWindow("mask_dst");
		//moveWindow("mask_dst", 3400, 0);
		imshow("mask_dst", mask_dst);
		waitKey(param.wait_time);
	}

	//利用卷积计算密度，密度大于一定阈值时可得液滴

	float kernel_radius = start_radius;
	Mat kernel;
	double kernelArea(0.0);
	Mat  intensity_dst;
	double maxV;
	int maxID[2];

	float ro = 0.97;
	vector<CIRCLE> valid_circles;
	vector<float> score_index;
	int count(0);
	int round(0);

	while (kernel_radius > min_radius)
	{
		kernel = Mat::zeros(Size((int)kernel_radius * 2, (int)kernel_radius * 2), CV_8U);
		circle(kernel, Point(kernel_radius, kernel_radius), kernel_radius, 1, -1);
		//circle(kernel, Point(kernel_radius, kernel_radius), kernel_radius/2, 0, -1);//圆环卷积核
		kernelArea = countNonZero(kernel);


		filter2D(mask_dst, intensity_dst, CV_32F, kernel, Point(-1, -1), 0, BORDER_CONSTANT);

		//Mat allone = Mat::ones(mask_dst.size(), mask_dst.type());
		//Mat divideValue;
		//filter2D(allone, divideValue, CV_32F, kernel, Point(-1, -1), 0, BORDER_CONSTANT);

		//生成边界模板，比卷积速度快
		Mat divideValue = kernelArea* Mat::ones(mask_dst.size(), CV_32F);

		Mat kernel_copy;
		kernel.copyTo(kernel_copy);

		for (int m = 0; m != divideValue.rows; m++)
		{
			float* p = divideValue.ptr<float>(m);
			if (m < kernel_radius)
			{
				for (int n = 0; n != divideValue.cols; n++)
				{
					if (n < kernel_radius)
					{
						Rect cir_rect(n - kernel.rows / 2, m - kernel.rows / 2, kernel.rows, kernel.rows);
						Rect roi_rect = cir_rect&Rect(0, 0, mask_dst.cols - 1, mask_dst.rows - 1);

						Rect roi_cir_rect(roi_rect.x - cir_rect.x, roi_rect.y - cir_rect.y, roi_rect.width, roi_rect.height);

						Mat roi(kernel_copy, roi_cir_rect);
						p[n] = countNonZero(roi);
					}
					else if (n > kernel_radius&&n < divideValue.cols - 1 - kernel_radius)
					{
						p[n] = p[n - 1];
					}
					else if (n > divideValue.cols - 1 - kernel_radius)
					{
						p[n] = p[divideValue.cols - 1 - n];
					}
				}
			}
			else if (m > kernel_radius&&m < divideValue.rows - 1 - kernel_radius)
			{
				float* p_temp = divideValue.ptr<float>(m-1);
				for (int n = 0; n != divideValue.cols; n++)
				{
					p[n] = p_temp[n];
				}
			}
			else if (m > divideValue.rows - 1 - kernel_radius)
			{
				float* p_temp = divideValue.ptr<float>(divideValue.rows - 1 -m);
				for (int n = 0; n != divideValue.cols; n++)
				{
					p[n] = p_temp[n];
				}
			}

		}

		intensity_dst = intensity_dst / divideValue;

		intensity_dst = intensity_dst.mul(1 / 255.0);
		minMaxIdx(intensity_dst, NULL, &maxV, NULL, maxID);

		if (maxV > ro)
		{
			//cout << "Round " << round++ << ": " << "circling drops" << endl;
			//找到所有密度大于ro的位置，每个圆的得分为圆心密度值
			int channels = intensity_dst.channels();
			int rows = intensity_dst.rows;
			int cols = intensity_dst.cols * channels;
			for (int i = 0; i < rows; ++i)
			{
				float* p = intensity_dst.ptr<float>(i);
				float* pEdge = divideValue.ptr<float>(i);

				for (int j = 0; j < cols; ++j)
				{
					if (p[j] > ro)
					{
						Rect circle_rect(j - kernel_radius+0.5, i - kernel_radius+0.5, 2 * kernel_radius+0.5, 2 * kernel_radius+0.5);
						Rect interRect = circle_rect&Rect(0, 0, mask_dst.cols - 1, mask_dst.rows - 1);
						float interAreaRate = (float)interRect.area() / (float)circle_rect.area();
						

						float rate = pEdge[j] / (pi*kernel_radius*kernel_radius);
						if (rate> 0.5)//边界区域面积大于一半者有效
						{
							//cout << pEdge[j] << endl;

							const Mat roi_rect(dst_img, interRect);
							float roi_m = countNonZero(roi_rect);

							float singleScore = p[j] * pEdge[j] / roi_m;
							float score = p[j] * rate * singleScore;
							score = score < 1.0 ? score : 1.0;


							valid_circles.push_back(pair<Point2f, float>(Point2f(j, i), kernel_radius));
							score_index.push_back(score);
							count++;
						}
						circle(mask_dst, Point2f(j, i), kernel_radius, 0, -1);
					}
				}
			}
			if (isDisplay)
			{
				//namedWindow("mask_dst");
				//moveWindow("mask_dst", 3400, 0);
				imshow("mask_dst", mask_dst);
				waitKey();
			}
		}

		//kernel_radius = (float)kernel_radius*sqrt(maxV);

		float temp_radius = (float)kernel_radius*sqrt(maxV);
		kernel_radius = temp_radius < kernel_radius - 0.5 ? temp_radius : kernel_radius - 0.5;
	}

	//NMS过滤
	ScoreCircle inputCircles(valid_circles, score_index);
	NMS(inputCircles, scoreCircles);
	//scoreCircles.circles.assign(valid_circles.begin(), valid_circles.end());
	//scoreCircles.scores.assign(score_index.begin(), score_index.end());

	Mat drop_dst;
	dst_img.copyTo(drop_dst);
	cvtColor(drop_dst, drop_dst, COLOR_GRAY2BGR);
	for (int i = 0; i < scoreCircles.circles.size(); i++)
	{
		circle(drop_dst, scoreCircles.circles[i].first, scoreCircles.circles[i].second, Scalar(0, 0, 255), 1);
	}
	if (isDisplay)
	{
		//namedWindow("drop_dst");
		//moveWindow("drop_dst", 3800, 0);
		imshow("drop_dst", drop_dst);
		waitKey();
	}

}

void NMS(const ScoreCircle sCircles, ScoreCircle &outputScoreCircles, bool useArea)
{
	vector<CIRCLE> valid_circles;
	valid_circles.assign(sCircles.circles.begin(), sCircles.circles.end());
	vector<float> valid_scores;
	valid_scores.assign(sCircles.scores.begin(), sCircles.scores.end());

	vector<pair<float, int>> score_index;
	for (int i = 0; i != valid_scores.size(); i++)
	{
		if (useArea)
			score_index.push_back(pair<float, int>(valid_circles[i].second, i));
		else
			score_index.push_back(pair<float, int>(valid_scores[i], i));
	}

	//score从大到小排序
	sort(score_index.begin(), score_index.end(), greater<pair<float, int>>());

	/*非极大值抑制NMS，以两个圆iou作为iou*/
	vector<CIRCLE> nms_circles; //存nms后的circles
	vector<float> circle_scores; //存nms后的得分
	vector<int> delete_index;

	for (int i_ter = 0; i_ter != score_index.size(); i_ter++)
	{

		vector<int>::iterator id_iter = find(delete_index.begin(), delete_index.end(), score_index[i_ter].second);
		if (id_iter != delete_index.end())
			continue;
		pair<Point2f, float> circle_1 = valid_circles[score_index[i_ter].second];
		nms_circles.push_back(valid_circles[score_index[i_ter].second]);
		circle_scores.push_back(score_index[i_ter].first);
		for (int j_ter = i_ter + 1; j_ter != score_index.size(); j_ter++)
		{
			pair<Point2f, float> circle_2 = valid_circles[score_index[j_ter].second];
			float areaInt = intersectionArea(circle_1, circle_2);
			float area_1 = pi*circle_1.second*circle_1.second;
			float area_2 = pi*circle_2.second*circle_2.second;
			float areaUnion = area_1 + area_2 - areaInt;
			float IOU = areaInt / areaUnion;
			if (IOU > 0.1 || areaInt / area_1 > 0.1 || areaInt / area_2 > 0.1)
				delete_index.push_back(score_index[j_ter].second);
		}
	}
	outputScoreCircles.circles.assign(nms_circles.begin(), nms_circles.end());
	outputScoreCircles.scores.assign(circle_scores.begin(), circle_scores.end());
}

//计算两个圆的相交面积
float intersectionArea(CIRCLE circle_1, CIRCLE circle_2)
{
	Point2f a = circle_1.first;
	float r1 = circle_1.second;
	Point2f b = circle_2.first;
	float r2 = circle_2.second;
	float d = sqrt((a.x - b.x)*(a.x - b.x) + (a.y - b.y)*(a.y - b.y));
	if (d >= r1 + r2)
		return 0;
	if (r1>r2)
	{
		float tmp = r1;
		r1 = r2;
		r2 = tmp;
	}
	if (r2 - r1 >= d)
		return pi*r1*r1;
	float ang1 = acos((r1*r1 + d*d - r2*r2) / (2 * r1*d));
	float ang2 = acos((r2*r2 + d*d - r1*r1) / (2 * r2*d));
	return ang1*r1*r1 + ang2*r2*r2 - r1*d*sin(ang1);
}

void mergeDropResult(const Mat &src_bw_img, ScoreCircle contected_drop_circles, ScoreCircle single_drop_circles, ScoreCircle &mergedCircles, const Parameter &param)
{
	bool show(0);
	Mat bw_img;
	src_bw_img.copyTo(bw_img);

	vector<CIRCLE> drop_circles = contected_drop_circles.circles;
	vector<CIRCLE> single_circles = single_drop_circles.circles;
	vector<int> double_index(drop_circles.size(), 0);
	for (int i = 0; i != drop_circles.size(); i++)
	{
		CIRCLE circle_big = drop_circles[i];
		for (int j = 0; j != single_circles.size(); j++)
		{
			CIRCLE circle_small = single_circles[j];
			float areaInt = intersectionArea(circle_big, circle_small);
			float area_1 = pi*circle_big.second*circle_big.second;
			float area_2 = pi*circle_small.second*circle_small.second;
			float areaUnion = area_1 + area_2 - areaInt;
			float IOU = areaInt / areaUnion;
			if (areaInt / area_2 >0.2&&IOU < 0.5)
				double_index[i]++;
		}
	}
	ScoreCircle outputCircles;
	for (int i = 0; i != double_index.size(); i++)
	{
		if (double_index[i] <= 1)
		{
			circle(bw_img, drop_circles[i].first, drop_circles[i].second, 0, -1);
			outputCircles.circles.push_back(drop_circles[i]);
			outputCircles.scores.push_back(contected_drop_circles.scores[i]);
		}
		else
		{
			circle(bw_img, drop_circles[i].first, drop_circles[i].second / 4, 0, -1);
		}
	}

	int kernelSize = param.kernel_size;
	Mat element_dilate = getStructuringElement(MORPH_ELLIPSE, Size(kernelSize, kernelSize));
	Mat element_erode = getStructuringElement(MORPH_ELLIPSE, Size(kernelSize, kernelSize));
	
	dilate(bw_img, bw_img, element_dilate);//膨胀 
	erode(bw_img, bw_img, element_erode);  //腐蚀 
	

	if (show || param.parameter_adjust)
	{
		imshow("test", bw_img);
		waitKey();
	}

	Mat showResult;
	bw_img.copyTo(showResult);

	Mat bw_img_all;
	src_bw_img.copyTo(bw_img_all);

	Mat bw_img_part;
	bw_img.copyTo(bw_img_part);

	ScoreCircle circles_new;

	vector<vector<Point>> pcountours;
	extractContours(bw_img, pcountours, param);

	for (int i = 0; i != pcountours.size(); i++)
	{
		Point2f center;
		float radius(0);
		//轮廓外接圆半径和圆心
		minEnclosingCircle(Mat(pcountours[i]), center, radius);
		float area_whole_circle = pi*radius*radius;  //在边界的非全圆，面积计算失效

		//计算外接圆的外接正方形roi
		Rect rect((int)(center.x - radius), (int)(center.y - radius), (int)(2 * radius), (int)(2 * radius));
		Rect roi_rect = rect&Rect(0, 0, bw_img.size().width - 1, bw_img.size().height - 1);

		//内接圆内全白的正方形mask
		Mat zeros_img = Mat::zeros(bw_img.size(), bw_img.type());// 全0图
		circle(zeros_img, center, radius, 1, -1);
		Mat roi_circle_mask(zeros_img, roi_rect);
		float area_circle = countNonZero(roi_circle_mask);  //mask面积，即在不同情况下圆mask的面积

		if (area_circle < pi*param.min_radius*param.min_radius)//面积过小者丢弃
			continue;
		if (area_circle / area_whole_circle < 0.5) //边界液滴拟合圆不满半圆者丢弃
			continue;


		Mat roi_all(bw_img_all, roi_rect); //未切割前bw
		Mat roi_part(bw_img_part, roi_rect);//切割后bw

		Mat mask = Mat::zeros(roi_all.size(), roi_all.type());
		circle(mask, Point((int)radius,(int)radius), radius, 1, -1); //mask

		Mat roi_circle;
		roi_all.copyTo(roi_circle,mask);
		float nonzeros = countNonZero(roi_circle);

		Mat _roi_part, fill_roi_part;
		roi_part.copyTo(_roi_part, mask);
		fillHole(_roi_part, fill_roi_part);

		//密度收缩搜索液滴
		ScoreCircle circles_temp;
		findDrops(fill_roi_part, circles_temp, param);//使用空洞填充后的roi进行密度收缩

		for (int k = 0; k != circles_temp.circles.size(); k++)
		{
			circles_new.circles.push_back(CIRCLE(circles_temp.circles[k].first + Point2f(roi_rect.x, roi_rect.y), circles_temp.circles[k].second + 1));
			circles_new.scores.push_back(circles_temp.scores[k]);
		}

		if (nonzeros / area_circle > param.area_rate&&area_circle<pi*param.max_radius*param.max_radius) //单独液滴
		{
			circles_new.circles.push_back(CIRCLE(center, radius));
			//计算得分
			Mat square_roi(src_bw_img, roi_rect);
			float m_square = countNonZero(square_roi);
			float single_score = nonzeros / m_square;
			single_score = single_score < 1.0 ? single_score : 1.0;
			float score = nonzeros / area_circle*single_score;
			circles_new.scores.push_back(score);
		}
	}

	outputCircles.circles.insert(outputCircles.circles.end(), circles_new.circles.begin(), circles_new.circles.end());
	outputCircles.scores.insert(outputCircles.scores.end(), circles_new.scores.begin(), circles_new.scores.end());

	//cvtColor(bw_img, bw_img, CV_GRAY2BGR);

	for (int i = 0; i != circles_new.circles.size(); i++)
	{
		circle(bw_img_part, circles_new.circles[i].first, circles_new.circles[i].second, 0, -1);
	}

	erode(bw_img_part, bw_img_part, element_erode);  //腐蚀 
	dilate(bw_img_part, bw_img_part, element_dilate);//膨胀 


	if (show || param.parameter_adjust)
	{
		cvtColor(showResult, showResult, COLOR_GRAY2BGR);
		for (int i = 0; i != circles_new.circles.size(); i++)
		{
			circle(showResult, circles_new.circles[i].first, circles_new.circles[i].second, Scalar(0, 0, 255), 1);
		}

		imshow("showResult", showResult);
		waitKey();
	}
	ScoreCircle circles_temp;

	if (param.parameter_adjust)
	{
		imshow("bw_img_part", bw_img_part);
		waitKey();
	}

	findSingleDrop(bw_img_part, circles_temp, param);//再执行一遍单液滴识别

	if (param.parameter_adjust)
	{
		cvtColor(bw_img_part, bw_img_part, COLOR_GRAY2BGR);
		for (int i = 0; i != circles_temp.circles.size(); i++)
		{
			circle(bw_img_part, circles_temp.circles[i].first, circles_temp.circles[i].second, Scalar(0, 0, 255), 1);
		}
		imshow("result", bw_img_part);
		waitKey();
	}

	outputCircles.circles.insert(outputCircles.circles.end(), circles_temp.circles.begin(), circles_temp.circles.end());
	outputCircles.scores.insert(outputCircles.scores.end(), circles_temp.scores.begin(), circles_temp.scores.end());
	
	//mergedCircles.circles.assign(outputCircles.circles.begin(), outputCircles.circles.end());
	//mergedCircles.scores.assign(outputCircles.scores.begin(), outputCircles.scores.end());
	NMS(outputCircles, mergedCircles,0);
}

//空洞填充
void fillHole(const Mat src_bw, Mat &dst_bw)
{
	Mat Temp = Mat::zeros(src_bw.size().height + 2, src_bw.size().width + 2, src_bw.type());//延展图像  
	src_bw.copyTo(Temp(Range(1, src_bw.size().height + 1), Range(1, src_bw.size().width + 1)));

	cv::floodFill(Temp, Point(0, 0), Scalar(255));

	Mat cutImg;//裁剪延展的图像  
	Temp(Range(1, src_bw.size().height + 1), Range(1, src_bw.size().width + 1)).copyTo(cutImg);

	dst_bw = src_bw | (~cutImg);
}

//得到分割后的图像和单连通域（明场）
void extractContoursBright(const Mat& src_gray, vector<vector<Point>>& pCountour_all, int kernelSize, const double area_rate, const double min_area, const double max_area, bool parameter_adjust)
{
	Mat element_dilate = getStructuringElement(MORPH_ELLIPSE, Size(kernelSize, kernelSize));
	Mat element_erode = getStructuringElement(MORPH_ELLIPSE, Size(kernelSize, kernelSize));
	Mat bw_img;

	adaptiveThreshold(src_gray, bw_img, 255, ADAPTIVE_THRESH_GAUSSIAN_C, THRESH_BINARY, 101, 0);//自适应二值化
	erode(bw_img, bw_img, element_erode);  //腐蚀 
	dilate(bw_img, bw_img, element_dilate); //膨胀 
	if (parameter_adjust)
	{
		imshow("bw_img", bw_img);  //中间结果展示
		waitKey();
	}

	Mat bw_img_copy;
	bw_img.copyTo(bw_img_copy);

	vector<vector<Point>> temp_countour;
	findContours(bw_img_copy, temp_countour, RETR_LIST, CHAIN_APPROX_SIMPLE, Point(0, 0));

	for (int i = 0; i < temp_countour.size(); i++)
	{
		double area = contourArea(temp_countour[i]);
		if (area<min_area || area>max_area)
			continue;

		//轮廓rect
		Rect roi_rect = boundingRect(temp_countour[i]) & Rect(0, 0, bw_img.size().width - 1, bw_img.size().height - 1);

		//轮廓内部填充白色的roi
		Mat zeros_img = Mat::zeros(bw_img.size(), bw_img.type());
		drawContours(zeros_img, temp_countour, i, 255, -1);
		Mat roi_countour = zeros_img(roi_rect);

		//二值图roi
		const Mat roi_src(bw_img, roi_rect);

		Mat countour_src;
		multiply(roi_countour, roi_src, countour_src);

		//轮廓内部白色点数
		int nonzeros = countNonZero(countour_src);

		//轮廓内部白色点数与轮廓区域面积占比，避免出现黑色填充的轮廓或者白色圆环
		if ((double)nonzeros / contourArea(temp_countour[i]) > area_rate)
		{
			pCountour_all.push_back(temp_countour[i]);
		}
	}
}