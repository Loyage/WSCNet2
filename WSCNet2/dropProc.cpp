#include "dropProc.hpp"

#include <io.h>
#include <opencv2/opencv.hpp>
#include "includes/nlohmann/json.hpp"

using namespace cv;
using namespace std;
using json = nlohmann::json;

void readImgNames(const string& img_path, vector<string>& img_names, const vector<string>& img_ext_vec)
{
	struct _finddata_t fileinfo;
	long long handle;
	handle = _findfirst((img_path + "*.*").c_str(), &fileinfo);
	if (handle != -1)
	{
		do
		{
			string filename = fileinfo.name;
			string ext = filename.substr(filename.find_last_of(".") + 1);
			if (img_ext_vec.end() != find(img_ext_vec.begin(), img_ext_vec.end(), ext))
			{
				cout << filename << endl;
				img_names.push_back(filename);
			}

		} while (_findnext(handle, &fileinfo) == 0);
	}
	if (img_names.empty())
	{
		cout << "\n fail to find images! \n" << endl;
		system("pause");
	}
	_findclose(handle);

	return;
}

int dropFindBright(const Mat& src_gray, const Parameter& param, int dev, ScoreCircle& final_circles)
{
	// 模板匹配方法
	vector<vector<Point>> pCountour; //正向二值化处理
	double min_area = pi * param.min_radius * param.min_radius;
	double max_area = pi * param.max_radius * param.max_radius;
	extractContoursBright(src_gray, pCountour, param.kernel_size, param.area_rate, min_area, max_area, param.parameter_adjust);
	vector<vector<Point>> pCountour_all;  //合并轮廓
	pCountour_all.insert(pCountour_all.end(), pCountour.begin(), pCountour.end());
	vector<vector<Point>> sorted_pCountour_all;
	vector<pair<int, int>> countour_size;

	for (int i = 0; i != pCountour_all.size(); i++)
	{
		pair<int, int> sizeWithIndex;
		sizeWithIndex.first = pCountour_all[i].size();
		sizeWithIndex.second = i;
		countour_size.push_back(sizeWithIndex);
	}

	sort(countour_size.begin(), countour_size.end()); //对轮廓大小排序
	for (vector<pair<int, int>>::iterator iter = countour_size.begin(); iter != countour_size.end(); iter++)
	{
		sorted_pCountour_all.push_back(pCountour_all[iter->second]);
	}

	Mat drops_img = Mat::zeros(src_gray.size(), src_gray.type());
	Point2f center;
	float radius(0);
	float area_circle(1e-5);
	float area_rate(0);
	double area(0);
	int count(0);
	for (int i = 0; i != pCountour_all.size(); i++)
	{
		area = contourArea(pCountour_all[i]);
		if (area < min_area || area > max_area) //液滴大小过滤，像素面积  ,可修改
			continue;

		minEnclosingCircle(Mat(pCountour_all[i]), center, radius);
		area_circle = 3.1416 * radius * radius;
		area_rate = area / area_circle;
		if (area_rate < param.area_rate) //拟合成圆的像素面积占比过滤
			continue;

		float x = center.x - sqrt(2) / 2 * radius;
		float y = center.y - sqrt(2) / 2 * radius;
		float  square_edge = sqrt(2) * radius;
		Rect rect_inside((int)x, (int)y, (int)square_edge, (int)square_edge);  //内接正方形roi
		Rect rect(0, 0, drops_img.size().width - 1, drops_img.size().height - 1);
		const Mat roi = drops_img(rect_inside & rect);

		int nonzeros = countNonZero(roi);
		if (nonzeros > 0) //覆盖的圆过滤，筛除大圆
		{
			continue;
		}
		count++;

		drawContours(drops_img, pCountour_all, i, Scalar(255), -1);

		final_circles.circles.push_back(make_pair(center, radius + dev));
		final_circles.scores.push_back(.0);
	}


	// Hough变换方法，作为补充
	vector<Vec3f> circles_hough;
	vector<Vec3f> circles_add;
	Mat src_gray_blur;
	medianBlur(src_gray, src_gray_blur, 3);
	double min_dist = param.min_radius + param.max_radius;
	HoughCircles(src_gray, circles_hough, HOUGH_GRADIENT, 1, 2*param.min_radius, 60.0, 18.0, param.min_radius, param.max_radius);
	for (const auto& circle_hough : circles_hough)
	{
		bool flag = true;
		for (int i = 0; i < final_circles.circles.size(); i++)
		{
			const auto& circle_final = final_circles.circles[i];
			const double dist = std::sqrtf(powf(circle_hough[0] - circle_final.first.x, 2)+ powf(circle_hough[1] - circle_final.first.y, 2));
			if (dist < param.min_radius)
			{
				flag = false;
				break;
			}
		}
		if (flag)
		{
			circles_add.push_back(circle_hough);
		}
	}
	for (const auto& circle_add : circles_add)
	{
		final_circles.circles.push_back(make_pair(Point2f(circle_add[0], circle_add[1]), circle_add[2] + dev/2));
		final_circles.scores.push_back(.0);
		count++;
	}

	return count;
}

int dropFindDark(const Mat& src_gray, const Parameter& param, ScoreCircle& final_circles)
{
	// 1. 分割得到两个阈值，三个中心
	// Step 1: Running  image segmantation by minimizing weighted sum of variances.
	GrayThreshold gthd;
	minVarSegmentation(src_gray, gthd, param);

	// 2. 剔除图像中的重叠液滴
	// Step 2: Searching overlapping droplets.
	Mat drops_without_dark;
	vector<CIRCLE> overlapCircles;
	findOverlaps(src_gray, gthd, param, drops_without_dark, overlapCircles);

	// 3. 利用连通域分割液滴，可用于下一步的填充空漏洞
	// Step 3: Separating valid droplet-area into single droplet by analyzing connected components.
	ScoreCircle single_drop_circles;
	Mat single_drop_img;
	drawSingleDrop(drops_without_dark, single_drop_img, single_drop_circles, param, gthd);

	// 4. 利用密度收缩算法分割液滴
	// Step 4: Separating valid droplet-area into single droplet by exploiting density shrink algorithm.
	ScoreCircle connected_drop_circles;
	Mat bw_drop_img;
	findDroplet(drops_without_dark, single_drop_img, connected_drop_circles, bw_drop_img, param, gthd);

	// 5. 合并3和4的结果，优化结果
	// Step 5: Merging the results between step 3 and step 4.
	mergeDropResult(bw_drop_img, connected_drop_circles, single_drop_circles, final_circles, param);

	return final_circles.circles.size();
}

void drawDropCircle(Mat& src_img, int drop_x, int drop_y, int drop_r, int drop_label)
{
	switch (drop_label)
	{
	case -1:
		break;
	case 0:
		circle(src_img, Point(drop_x, drop_y), drop_r, Scalar(0, 0, 255), 1); // Red for Empty drop
		break;
	case 1:
		circle(src_img, Point(drop_x, drop_y), drop_r, Scalar(255, 0, 0), 1); // Blue for Single drop
		break;
	case 2:
		circle(src_img, Point(drop_x, drop_y), drop_r, Scalar(0, 255, 0), 1); // Green for Multiple drop
		break;
	default:
		break;
	}

	return;
}
