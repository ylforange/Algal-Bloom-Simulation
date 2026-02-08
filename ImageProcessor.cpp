// ImageProcessor.cpp（数据预处理）
#include "ImageProcessor.h"
#include <iostream>
#include "gdal_priv.h"

ImageProcessor::ImageProcessor(const std::string& image_path) {
    GDALAllRegister();
    GDALDataset* poDataset = (GDALDataset*)GDALOpen(image_path.c_str(), GA_ReadOnly);

    if (poDataset == NULL) {
        std::cerr << "错误：GDAL无法打开图像，路径: " << image_path << std::endl;
        loaded_successfully_ = false;
        return;
    }

    int width = poDataset->GetRasterXSize();
    int height = poDataset->GetRasterYSize();
    int num_bands = poDataset->GetRasterCount();

    GDALDataType gdal_type = poDataset->GetRasterBand(1)->GetRasterDataType();
    int opencv_type = -1;

    //将GDAL的数据类型映射到OpenCV的数据类型
    switch (gdal_type) {
    case GDT_Byte:    opencv_type = CV_8U;  break;  // 8位无符号整型
    case GDT_UInt16:  opencv_type = CV_16U; break;  // 16位无符号整型
    case GDT_Int16:   opencv_type = CV_16S; break;  // 16位有符号整型
    case GDT_UInt32:  opencv_type = CV_32S; break;  // 32位无符号整型 (OpenCV用有符号代替)
    case GDT_Int32:   opencv_type = CV_32S; break;  // 32位有符号整型
    case GDT_Float32: opencv_type = CV_32F; break;  // 32位浮点型
    case GDT_Float64: opencv_type = CV_64F; break;  // 64位浮点型 (double)
    default:
        std::cerr << "错误：不支持的GDAL数据类型: " << GDALGetDataTypeName(gdal_type) << std::endl;
        loaded_successfully_ = false;
        GDALClose(poDataset);
        return;
    }

    std::cout << "检测到GDAL数据类型: " << GDALGetDataTypeName(gdal_type) << ", 对应OpenCV类型: " << opencv_type << std::endl;

    std::vector<cv::Mat> bands_vector;
    for (int i = 1; i <= num_bands; ++i) {
        GDALRasterBand* poBand = poDataset->GetRasterBand(i);

        cv::Mat current_band(height, width, opencv_type);

        CPLErr read_result = poBand->RasterIO(GF_Read, 0, 0, width, height,
            current_band.data, width, height, gdal_type,
            0, 0);

        if (read_result != CE_None) {
            std::cerr << "错误：从波段 " << i << " 读取数据失败!" << std::endl;
            loaded_successfully_ = false;
            GDALClose(poDataset);
            return;
        }

        bands_vector.push_back(current_band);
    }

    cv::merge(bands_vector, image_data_);

    loaded_successfully_ = true;
    GDALClose(poDataset);

    std::cout << "成功加载图像: " << image_path << std::endl;
    std::cout << "尺寸: " << width << "x" << height << ", 通道数: " << image_data_.channels() << std::endl;
}

bool ImageProcessor::isLoaded() const {
    return loaded_successfully_;
}

cv::Mat ImageProcessor::calculateNDVI() {
    if (!loaded_successfully_ || image_data_.channels() < 2) {
        std::cerr << "错误：无法计算NDVI。图像未加载或波段数少于2。" << std::endl;
        return cv::Mat(); 
    }

    std::vector<cv::Mat> bands = this->getBands();

    cv::Mat red_band = bands[image_data_.channels() - 2];
    cv::Mat nir_band = bands[image_data_.channels() - 1];

    cv::Mat red_float, nir_float;
    red_band.convertTo(red_float, CV_32F);
    nir_band.convertTo(nir_float, CV_32F);

    cv::Mat numerator = nir_float - red_float;
    cv::Mat denominator = nir_float + red_float;

    cv::Mat ndvi;
    cv::divide(numerator, denominator, ndvi);

    return ndvi;
}

cv::Mat ImageProcessor::extractAlgaeMask(const cv::Mat& ndvi_image) {
    if (ndvi_image.empty() || ndvi_image.type() != CV_32F) {
        std::cerr << "错误：extractAlgaeMask函数的输入无效。需要一个单通道浮点型Mat。" << std::endl;
        return cv::Mat();
    }

    cv::Mat algae_mask;
    cv::threshold(ndvi_image, algae_mask, 0.0, 255.0, cv::THRESH_BINARY);

    cv::Mat algae_mask_8u;
    algae_mask.convertTo(algae_mask_8u, CV_8U);

    return algae_mask_8u;
}

std::vector<cv::Mat> ImageProcessor::getBands() const {
    std::vector<cv::Mat> bands;
    if (loaded_successfully_) {
        cv::split(image_data_, bands);
    }
    return bands;
}

cv::Mat ImageProcessor::createNDVIColorMap(const cv::Mat& ndvi_image) {
    if (ndvi_image.empty() || ndvi_image.type() != CV_32F) {
        std::cerr << "错误：createNDVIColorMap 的输入无效，需要一个单通道浮点型Mat。" << std::endl;
        return cv::Mat();
    }

    cv::Mat colormap_image = cv::Mat(ndvi_image.size(), CV_8UC3);

    for (int y = 0; y < ndvi_image.rows; ++y) {
        for (int x = 0; x < ndvi_image.cols; ++x) {
            float ndvi_value = ndvi_image.at<float>(y, x);
            cv::Vec3b& pixel = colormap_image.at<cv::Vec3b>(y, x);

            if (ndvi_value < 0) { // 水体
                // 线性插值来模拟颜色渐变
                float ratio = -ndvi_value; // ratio in [0, 1]
                uchar blue = (uchar)(150 * (1 - ratio) + 200 * ratio);
                uchar green = (uchar)(100 * (1 - ratio) + 220 * ratio);
                uchar red = (uchar)(50 * (1 - ratio) + 180 * ratio);
                pixel = cv::Vec3b(blue, green, red);
            }
            else { // 藻华/陆地
                // [0, 1]映射为一个从浅绿到深绿的渐变
                float ratio = ndvi_value; // ratio in [0, 1]
                uchar blue = (uchar)(20 * (1 - ratio) + 0 * ratio);
                uchar green = (uchar)(220 * (1 - ratio) + 80 * ratio);
                uchar red = (uchar)(20 * (1 - ratio) + 0 * ratio);
                pixel = cv::Vec3b(blue, green, red);
            }
        }
    }
    return colormap_image;
}

cv::Mat ImageProcessor::createDataMask() {
    if (!loaded_successfully_) {
        return cv::Mat();
    }
    std::vector<cv::Mat> bands = this->getBands();

    cv::Mat mask = cv::Mat::zeros(image_data_.size(), CV_8U);
    cv::Mat accumulator = cv::Mat::zeros(image_data_.size(), CV_32F);

    for (const auto& band : bands) {
        cv::Mat band_float;
        band.convertTo(band_float, CV_32F);
        accumulator += band_float;
    }

    cv::threshold(accumulator, mask, 0, 255, cv::THRESH_BINARY);

    cv::Mat final_mask;
    mask.convertTo(final_mask, CV_8U);

    return final_mask;
}

cv::Mat createColorMapFromMask(const cv::Mat& mask, const cv::Mat& background_template) {
    cv::Mat colormap = background_template.clone();
    colormap.setTo(cv::Scalar(0, 200, 0), mask);
    return colormap;
}