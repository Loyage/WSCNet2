#include "dropRecgThread.h"

#ifdef WIN32
#pragma execution_character_set("utf-8")
#endif

#include <QFile>
#include <QDebug>
#include <QFileInfo>

#include <fstream>
#include <direct.h>
#include <chrono>

#include <opencv2/opencv.hpp>

#include "dropProc.hpp"
#include "includes/nlohmann/json.hpp"

#define NOMINMAX
#undef min
#undef max
#undef slots
#include <torch/torch.h>
#include <torch/script.h>
#define slots Q_SLOTS

using namespace std;
using namespace cv;
using json = nlohmann::json;

// 在源文件中定义该函数，避免在头文件中包含torch而影响编译时长
void dropMatToTensor(const Mat& drop_img, torch::Tensor& img_tensor);
void findCells(const Mat& input_mat, vector<Point>& cell_pos);

dropRecgThread::dropRecgThread(QObject* parent) : QThread(parent)
{
}

dropRecgThread::~dropRecgThread()
{
}

void dropRecgThread::setPathToSaveResult(QString img_save_folder, QString text_save_folder)
{
	m_img_save_folder = img_save_folder;
	m_text_save_folder = text_save_folder;
}

void dropRecgThread::setCountObject(ECountMode count_mode, QString path)
{
	m_count_mode = count_mode;
	if (count_mode == ECountMode::COUNT_FOLDER)
	{
		m_folder_path = path;
		m_file_name = "";
	}
	else if (count_mode == ECountMode::COUNT_IMG)
	{
		// 取path的路径部分
		m_folder_path = (QFileInfo(path).path() + "/");
		m_file_name = (QFileInfo(path).fileName());
	}
	else if (count_mode == ECountMode::COUNT_VIDEO)
	{
		m_folder_path = (QFileInfo(path).path() + "/");
		m_file_name = (QFileInfo(path).fileName());
	}
}

void dropRecgThread::setParams(bool is_bright_field, int kernel_size, int min_radius, int max_radius, int dev_bright, QString module_path)
{
	m_is_bright_field = is_bright_field;
	m_module_path = module_path;
	m_dev_bright = dev_bright;
	m_kernel_size = kernel_size;
	m_min_radius = (float)min_radius;
	m_max_radius = (float)max_radius;
	m_is_WSCNet = m_module_path.contains("WSCNet");
}

void dropRecgThread::run()
{
	bool is_without_module = m_module_path.isEmpty();
	string img_folder = m_folder_path.toStdString();
	if (m_count_mode == ECountMode::COUNT_VIDEO)
	{
		videoToFrames(img_folder); // 将视频转换为帧，并且更改img_folder
	}

	//create folder
	_mkdir((img_folder + m_text_save_folder.toStdString()).c_str());
	_mkdir((img_folder + m_img_save_folder.toStdString()).c_str());

	//load parameters
	const bool is_find_overlap = 1;
	const float area_rate = 0.5;
	const bool parameter_adjust = false;
	const bool visualization = false;
	const int wait_time = 0;
	Parameter param(m_kernel_size, m_min_radius, m_max_radius, area_rate, is_find_overlap, parameter_adjust, visualization, wait_time);

	// load module
	torch::jit::Module clsfy_module;
	auto device = torch::Device(torch::kCUDA, 0);
	if (!is_without_module)
	{
		reportToMain(tr("正在加载分类模型..."));
		if (!torch::cuda::is_available())
		{
			emit reportToMain("<font color=\"#FF8000\">WARNING</font> CUDA is not available, use CPU instead!");
		}

		if (QFile::exists(m_module_path))
		{
			emit reportToMain("Using module: \n" + m_module_path);
		}
		else
		{
			emit reportToMain("<font color=\"#FF0000\">ERROR</font> " + QObject::tr("未能找到module，请检查模型路径！"));
			emit done();
		}

		try
		{
			clsfy_module = torch::jit::load(m_module_path.toStdString()); // possibly cause exception in Debug mode
			clsfy_module.to(device);
			clsfy_module.eval();
		}
		catch (const exception&)
		{
			emit reportToMain("<font color=\"#FF0000\">ERROR</font> " + QObject::tr("模型加载错误！"));
			emit done();
		}
	}

	// load img names for different count mode
	vector<std::string> img_names;
	if (m_count_mode == ECountMode::COUNT_IMG)
	{
		img_names.push_back(m_file_name.toStdString());
	}
	else if (m_count_mode == ECountMode::COUNT_FOLDER)
	{
		const vector<string> img_ext_vec{ "bmp", "png", "jpg", "jpeg", "tif" };
		readImgNames(img_folder, img_names, img_ext_vec);
	}
	else if (m_count_mode == ECountMode::COUNT_VIDEO)
	{
		readImgNames(img_folder, img_names, vector<string>{ "png" });
	}
	else
	{
		emit reportToMain("<font color=\"#FF0000\">ERROR</font> Unkown count mode！");
		emit done();
	}

	if (img_names.empty())
	{
		emit reportToMain("<font color=\"#FF0000\">ERROR</font> " + tr("未能找到图片！"));
		emit done();
	}


	// main loop
	if (!is_without_module)
		reportToMain(tr("模型加载完成，开始识别..."));
	auto start_time = chrono::steady_clock::now();
	for (const auto& img_name : img_names)
	{
		string img_path = img_folder + img_name;
		Mat src_color = imread(img_path);
		Mat src_gray;
		cvtColor(src_color, src_gray, COLOR_RGB2GRAY);
		ScoreCircle final_circles;

		int drop_num = 0;
		if (m_is_bright_field)
		{
			drop_num = dropFindBright(src_gray, param, m_dev_bright, final_circles);
		}
		else
		{
			drop_num = dropFindDark(src_gray, param, final_circles);
		}

		if (drop_num == 0)
		{
			emit reportToMain("The number of drops in " + QString::fromStdString(img_name) + " is: 0");
			continue;
		}

		// 保存位置、判别结果和可视化图像
		int true_drop_num = 0;
		string img_name_cut = img_name.substr(0, img_name.find_last_of("."));
		string file_ext = m_is_WSCNet ? "_cells" : "_drops";
		string text_res_path = img_folder + m_text_save_folder.toStdString() + "\\" + img_name_cut + file_ext + ".txt";
		string img_res_path = img_folder + m_img_save_folder.toStdString() + "\\" + img_name_cut + file_ext + ".png";
		ofstream fout(text_res_path);
		Mat result_img = src_color.clone();

		// 使用模型，进行判别
		if (!is_without_module)
		{
			int drop_label_dict[4] = { -1, 0, 1, 2 }; // 0: empty drop; 1: single drop; 2: multi drop; -1: not a drop
			vector<torch::Tensor> drop_imgs;
			// 遍历液滴
			for (int drop_i = 0; drop_i < final_circles.size(); drop_i++)
			{
				int drop_x = static_cast<int>(final_circles.circles[drop_i].first.x);
				int drop_y = static_cast<int>(final_circles.circles[drop_i].first.y);
				int drop_r = static_cast<int>(final_circles.circles[drop_i].second);

				Rect rect(drop_x - drop_r + 1, drop_y - drop_r + 1, drop_r * 2 + 0.5, drop_r * 2 + 0.5);
				rect = rect & Rect(0, 0, src_gray.cols - 1, src_gray.rows - 1);

				Mat drop_img = src_color(rect).clone();
				torch::Tensor img_tensor;
				dropMatToTensor(drop_img, img_tensor); // 液滴图像Tensor化
				drop_imgs.push_back(img_tensor);
			}
			if (!m_is_WSCNet) // 普通卷积神经网络
			{
				auto output = clsfy_module.forward({ torch::cat(drop_imgs, 0).to(device) }).toTensor().to(torch::kCPU);
				auto arg_max = torch::argmax(output, 1); // 判别结果
				for (int drop_i = 0; drop_i < final_circles.size(); drop_i++)
				{
					int drop_x = static_cast<int>(final_circles.circles[drop_i].first.x);
					int drop_y = static_cast<int>(final_circles.circles[drop_i].first.y);
					int drop_r = static_cast<int>(final_circles.circles[drop_i].second);
					int drop_label = drop_label_dict[arg_max[drop_i].item<int>()];

					fout << drop_x << "\t" << drop_y << "\t" << drop_r << "\t" << drop_label << endl;

					drawDropCircle(result_img, drop_x, drop_y, drop_r, drop_label);

					if (drop_label != -1) true_drop_num++;
				}
			}
			else // WSCNet
			{
				auto output = clsfy_module.forward({ torch::cat(drop_imgs, 0).to(device) }).toTuple()->elements();
				auto output_class = torch::argmax(output.at(0).toTensor().to(torch::kCPU), 1); // 判别结果
				auto output_count = output.at(1).toTensor().to(torch::kCPU); // 计数结果
				for (int drop_i = 0; drop_i < final_circles.size(); drop_i++)
				{
					int drop_x = static_cast<int>(final_circles.circles[drop_i].first.x);
					int drop_y = static_cast<int>(final_circles.circles[drop_i].first.y);
					int drop_r = static_cast<int>(final_circles.circles[drop_i].second);
					int drop_class = output_class[drop_i].item<int>();
					int drop_label = 0;

					if (drop_class == 0) // 不是液滴
					{
						drop_label = -1;
						fout << drop_x << "\t" << drop_y << "\t" << drop_r << "\t" << drop_label << endl;
					}
					else if (drop_class == 1)// 是液滴
					{
						auto output_count_i = output_count[drop_i];
						Mat count_mat(cv::Size(32, 32), CV_32F, output_count_i.data_ptr());
						vector<Point> cell_pos_drop;
						findCells(count_mat, cell_pos_drop);
						drop_label = cell_pos_drop.size();

						fout << drop_x << "\t" << drop_y << "\t" << drop_r << "\t" << drop_label;
						drawDropCircle(result_img, drop_x, drop_y, drop_r, drop_label);

						// draw cells
						for (const auto& cell_pos : cell_pos_drop)
						{

							int cell_x = cell_pos.x;
							int cell_y = cell_pos.y;
							Rect drop_rect = Rect(drop_x - drop_r, drop_y - drop_r, 2 * drop_r, 2 * drop_r);
							drop_rect = drop_rect & Rect(0, 0, result_img.cols - 1, result_img.rows - 1);
							int cell_true_x = static_cast<int>((cell_x - 15.5) / 32.0 * drop_rect.width + drop_x);
							int cell_true_y = static_cast<int>((cell_y - 15.5) / 32.0 * drop_rect.height + drop_y);
							circle(result_img, Point(cell_true_x, cell_true_y), 1, Scalar(0, 165, 255), -1);
							fout << "\t" << cell_true_x << "\t" << cell_true_y;
						}
						fout << endl;
					}
					else return;
				}
			}

		}
		else // 不使用模型，仅进行分割
		{
			for (int drop_i = 0; drop_i < final_circles.size(); drop_i++)
			{
				int drop_x = static_cast<int>(final_circles.circles[drop_i].first.x);
				int drop_y = static_cast<int>(final_circles.circles[drop_i].first.y);
				int drop_r = static_cast<int>(final_circles.circles[drop_i].second);

				fout << drop_x << "\t" << drop_y << "\t" << drop_r << "\t" << 0 << endl;

				drawDropCircle(result_img, drop_x, drop_y, drop_r, 0);

				true_drop_num++;
			}
		}
		fout.close();
		imwrite(img_res_path, result_img);
		QString message = "The number of drops in " + QString::fromStdString(img_name) +
			" is: " + QString::fromStdString(to_string(true_drop_num)) + "\n";
		emit reportToMain(message);
	}

	if (m_count_mode == ECountMode::COUNT_VIDEO) // 将帧合成为视频
	{
		framesToVideo();
	}

	auto end_time = chrono::steady_clock::now();
	auto duration = chrono::duration_cast<chrono::milliseconds>(end_time - start_time);
	if (img_names.size() > 1)
	{
		reportToMain("Total image count: " + QString::fromStdString(to_string(img_names.size())));
		reportToMain("Total time: " + QString::fromStdString(to_string(duration.count())) + "ms");
		reportToMain("Average time: " + QString::fromStdString(to_string(duration.count() / img_names.size())) + "ms");
	}

	emit done();
}

void dropRecgThread::videoToFrames(std::string& img_folder)
{
	QString file_pure_name = m_file_name.left(m_file_name.indexOf(".", 0));
	img_folder = (m_folder_path + file_pure_name + "_frames/").toStdString();
	_mkdir(img_folder.c_str());

	VideoCapture capture;
	capture.open((m_folder_path + m_file_name).toStdString());
	if (!capture.isOpened()) {
		emit reportToMain("<font color=\"#FF0000\">ERROR</font> can't load video data！");
		emit done();
	}
	int frame_num = capture.get(CAP_PROP_FRAME_COUNT);

	Mat frame;
	for (int i = 0; i < frame_num; i++)
	{
		stringstream ss;
		string str;
		capture >> frame;
		ss << setw(6) << setfill('0') << i;
		ss >> str;
		imwrite(img_folder + "\\" + file_pure_name.toStdString() + str + ".png", frame);
	}
	capture.release();
}

void dropRecgThread::framesToVideo()
{
	QString file_pure_name = m_file_name.left(m_file_name.indexOf(".", 0));
	QString file_ext = m_file_name.right(m_file_name.length() - m_file_name.indexOf(".", 0));
	reportToMain(tr("正在将帧合成为视频..."));
	reportToMain("视频保存路径：" + m_folder_path + file_pure_name + "_drops" + file_ext);

	QString img_folder = (m_folder_path + file_pure_name + "_frames/" + m_img_save_folder + "/");

	VideoCapture capture;
	capture.open((m_folder_path + m_file_name).toStdString());
	double fps = capture.get(CAP_PROP_FPS);
	Size size = Size(capture.get(CAP_PROP_FRAME_WIDTH), capture.get(CAP_PROP_FRAME_HEIGHT));
	capture.release();

	VideoWriter writer;
	writer.open((m_folder_path + file_pure_name + "_drops" + file_ext).toStdString(), VideoWriter::fourcc('M', 'J', 'P', 'G'), fps, size);
	if (!writer.isOpened())
	{
		emit reportToMain("<font color=\"#FF0000\">ERROR</font> can't load video data！");
		emit done();
	}

	const vector<string> img_ext_vec{ "bmp", "png", "jpg", "jpeg", "tif" };
	vector<string> img_names;
	readImgNames(img_folder.toStdString(), img_names, img_ext_vec);

	for (const auto& img_name : img_names)
	{
		string img_path = img_folder.toStdString() + img_name;
		Mat img = imread(img_path);
		writer << img;
	}
	writer.release();
}

void findCells(const Mat& input_mat, vector<Point>& cell_pos)
{
	Scalar mat_sum = sum(input_mat);
	Mat temp_mat = input_mat.clone();
	int cell_num = floor(mat_sum[0]);
	for (int i = 0; i < cell_num; i++)
	{
		Point cell_pos_i;
		minMaxLoc(temp_mat, NULL, NULL, NULL, &cell_pos_i);
		temp_mat.at<float>(cell_pos_i) = 0;
		cell_pos.push_back(cell_pos_i);
	}
}

//将Mat转换为Tensor，并进行归一化
void dropMatToTensor(const Mat& drop_img, torch::Tensor& img_tensor)
{
	Mat drop_img_resize;
	resize(drop_img, drop_img_resize, Size(32, 32));
	img_tensor = torch::from_blob(drop_img_resize.data, { drop_img_resize.rows, drop_img_resize.cols, 3 }, torch::kByte);
	img_tensor = img_tensor.permute({ 2, 0, 1 });
	img_tensor = img_tensor.unsqueeze(0);
	img_tensor = img_tensor.toType(torch::kFloat);
	img_tensor = img_tensor.div(255.0);
	torch::data::transforms::Normalize<torch::Tensor> norm({ .5, .5, .5 }, { .5, .5, .5 });
	img_tensor = norm(img_tensor);

	return;
}
