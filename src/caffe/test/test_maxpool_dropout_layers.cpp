// Copyright 2014 Sergio Guadarrama

#include <cstring>
#include <cuda_runtime.h>

#include "gtest/gtest.h"
#include "caffe/blob.hpp"
#include "caffe/common.hpp"
#include "caffe/filler.hpp"
#include "caffe/vision_layers.hpp"
#include "caffe/test/test_gradient_check_util.hpp"

#include "caffe/test/test_caffe_main.hpp"

namespace caffe {

template <typename Dtype>
class MaxPoolingDropoutTest : public ::testing::Test {
 protected:
  MaxPoolingDropoutTest()
      : blob_bottom_(new Blob<Dtype>()),
        blob_top_(new Blob<Dtype>()) {};
  virtual void SetUp() {
    Caffe::set_random_seed(1703);
    blob_bottom_->Reshape(2, 3, 6, 5);
    // fill the values
    FillerParameter filler_param;
    filler_param.set_value(1.);
    ConstantFiller<Dtype> filler(filler_param);
    filler.Fill(this->blob_bottom_);
    blob_bottom_vec_.push_back(blob_bottom_);
    blob_top_vec_.push_back(blob_top_);
  };
  virtual ~MaxPoolingDropoutTest() { delete blob_bottom_; delete blob_top_; }
  Blob<Dtype>* const blob_bottom_;
  Blob<Dtype>* const blob_top_;
  vector<Blob<Dtype>*> blob_bottom_vec_;
  vector<Blob<Dtype>*> blob_top_vec_;
};

typedef ::testing::Types<float, double> Dtypes;
TYPED_TEST_CASE(MaxPoolingDropoutTest, Dtypes);

TYPED_TEST(MaxPoolingDropoutTest, TestSetup) {
  LayerParameter layer_param;
  layer_param.set_kernelsize(3);
  layer_param.set_stride(2);
  PoolingLayer<TypeParam> layer(layer_param);
  layer.SetUp(this->blob_bottom_vec_, &(this->blob_top_vec_));
  DropoutLayer<TypeParam> dropout_layer(layer_param);
  dropout_layer.SetUp(this->blob_top_vec_, &(this->blob_top_vec_));
  EXPECT_EQ(this->blob_top_->num(), this->blob_bottom_->num());
  EXPECT_EQ(this->blob_top_->channels(), this->blob_bottom_->channels());
  EXPECT_EQ(this->blob_top_->height(), 3);
  EXPECT_EQ(this->blob_top_->width(), 2);
}


TYPED_TEST(MaxPoolingDropoutTest, CPUForward) {
  LayerParameter layer_param;
  layer_param.set_kernelsize(3);
  layer_param.set_stride(2);
  Caffe::set_mode(Caffe::CPU);
  PoolingLayer<TypeParam> layer(layer_param);
  layer.SetUp(this->blob_bottom_vec_, &(this->blob_top_vec_));
  layer.Forward(this->blob_bottom_vec_, &(this->blob_top_vec_));
  const TypeParam* top_data = this->blob_top_->cpu_data();
  TypeParam sum = 0.;
  for (int i = 0; i < this->blob_top_->count(); ++i) {
  	sum += top_data[i];
  }
  EXPECT_EQ(sum, this->blob_top_->count());
  // Dropout in-place
  DropoutLayer<TypeParam> dropout_layer(layer_param);
  dropout_layer.SetUp(this->blob_top_vec_, &(this->blob_top_vec_));
  dropout_layer.Forward(this->blob_top_vec_, &(this->blob_top_vec_));
  sum = 0.;
  TypeParam scale = 1. / (1. - layer_param.dropout_ratio());
  top_data = this->blob_top_->cpu_data();
  for (int i = 0; i < this->blob_top_->count(); ++i) {
  	sum += top_data[i];
  }
  EXPECT_GE(sum, 0);
  EXPECT_LE(sum, this->blob_top_->count()*scale);
}

TYPED_TEST(MaxPoolingDropoutTest, GPUForward) {
  LayerParameter layer_param;
  layer_param.set_kernelsize(3);
  layer_param.set_stride(2);
  Caffe::set_mode(Caffe::GPU);
  PoolingLayer<TypeParam> layer(layer_param);
  layer.SetUp(this->blob_bottom_vec_, &(this->blob_top_vec_));
  layer.Forward(this->blob_bottom_vec_, &(this->blob_top_vec_));
  const TypeParam* top_data = this->blob_top_->cpu_data();
  TypeParam sum = 0.;
  for (int i = 0; i < this->blob_top_->count(); ++i) {
  	sum += top_data[i];
  }
  EXPECT_EQ(sum, this->blob_top_->count());

  DropoutLayer<TypeParam> dropout_layer(layer_param);
  dropout_layer.SetUp(this->blob_top_vec_, &(this->blob_top_vec_));
  dropout_layer.Forward(this->blob_top_vec_, &(this->blob_top_vec_));
  sum = 0.;
  TypeParam scale = 1. / (1. - layer_param.dropout_ratio());
  top_data = this->blob_top_->cpu_data();
  for (int i = 0; i < this->blob_top_->count(); ++i) {
  	sum += top_data[i];
  }
  EXPECT_GE(sum, 0);
  EXPECT_LE(sum, this->blob_top_->count()*scale);
}

TYPED_TEST(MaxPoolingDropoutTest, CPUBackward) {
  LayerParameter layer_param;
  layer_param.set_kernelsize(3);
  layer_param.set_stride(2);
  Caffe::set_mode(Caffe::CPU);
  Caffe::set_phase(Caffe::TRAIN);
  PoolingLayer<TypeParam> layer(layer_param);
  layer.SetUp(this->blob_bottom_vec_, &(this->blob_top_vec_));
  layer.Forward(this->blob_bottom_vec_, &(this->blob_top_vec_));
  for (int i = 0; i < this->blob_top_->count(); ++i) {
    this->blob_top_->mutable_cpu_diff()[i] = 1.;
  }  
  layer.Backward(this->blob_top_vec_, true, &(this->blob_bottom_vec_));
  const TypeParam* bottom_diff = this->blob_bottom_->cpu_diff();
  TypeParam sum = 0.;
  for (int i = 0; i < this->blob_bottom_->count(); ++i) {
  	sum += bottom_diff[i];
  }
  EXPECT_EQ(sum, this->blob_top_->count());
  // Dropout in-place
  DropoutLayer<TypeParam> dropout_layer(layer_param);
  dropout_layer.SetUp(this->blob_top_vec_, &(this->blob_top_vec_));
  dropout_layer.Forward(this->blob_top_vec_, &(this->blob_top_vec_));
  dropout_layer.Backward(this->blob_top_vec_, true, &(this->blob_top_vec_));
  layer.Backward(this->blob_top_vec_, true, &(this->blob_bottom_vec_));
  TypeParam sum_with_dropout = 0.;
  bottom_diff = this->blob_bottom_->cpu_diff();
  for (int i = 0; i < this->blob_bottom_->count(); ++i) {
  	sum_with_dropout += bottom_diff[i];
  }
  EXPECT_GE(sum_with_dropout, sum);
}

TYPED_TEST(MaxPoolingDropoutTest, GPUBackward) {
  LayerParameter layer_param;
  layer_param.set_kernelsize(3);
  layer_param.set_stride(2);
  Caffe::set_mode(Caffe::GPU);
  Caffe::set_phase(Caffe::TRAIN);
  PoolingLayer<TypeParam> layer(layer_param);
  layer.SetUp(this->blob_bottom_vec_, &(this->blob_top_vec_));
  layer.Forward(this->blob_bottom_vec_, &(this->blob_top_vec_));
  for (int i = 0; i < this->blob_top_->count(); ++i) {
    this->blob_top_->mutable_cpu_diff()[i] = 1.;
  }  
  layer.Backward(this->blob_top_vec_, true, &(this->blob_bottom_vec_));
  const TypeParam* bottom_diff = this->blob_bottom_->cpu_diff();
  TypeParam sum = 0.;
  for (int i = 0; i < this->blob_bottom_->count(); ++i) {
  	sum += bottom_diff[i];
  }
  EXPECT_EQ(sum, this->blob_top_->count());
  // Dropout in-place
  DropoutLayer<TypeParam> dropout_layer(layer_param);
  dropout_layer.SetUp(this->blob_top_vec_, &(this->blob_top_vec_));
  dropout_layer.Forward(this->blob_top_vec_, &(this->blob_top_vec_));
  dropout_layer.Backward(this->blob_top_vec_, true, &(this->blob_top_vec_));
  layer.Backward(this->blob_top_vec_, true, &(this->blob_bottom_vec_));
  TypeParam sum_with_dropout = 0.;
  bottom_diff = this->blob_bottom_->cpu_diff();
  for (int i = 0; i < this->blob_bottom_->count(); ++i) {
  	sum_with_dropout += bottom_diff[i];
  }
  EXPECT_GE(sum_with_dropout, sum);
}

}