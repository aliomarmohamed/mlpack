/**
 * @file methods/ann/layer/mean_pooling_impl.hpp
 * @author Marcus Edel
 * @author Nilay Jain
 *
 * Implementation of the MeanPooling layer class.
 *
 * mlpack is free software; you may redistribute it and/or modify it under the
 * terms of the 3-clause BSD license.  You should have received a copy of the
 * 3-clause BSD license along with mlpack.  If not, see
 * http://www.opensource.org/licenses/BSD-3-Clause for more information.
 */
#ifndef MLPACK_METHODS_ANN_LAYER_MEAN_POOLING_IMPL_HPP
#define MLPACK_METHODS_ANN_LAYER_MEAN_POOLING_IMPL_HPP

// In case it hasn't yet been included.
#include "mean_pooling.hpp"

namespace mlpack {
namespace ann /** Artificial Neural Network. */ {

template<typename InputType, typename OutputType>
MeanPoolingType<InputType, OutputType>::MeanPoolingType()
{
  // Nothing to do here.
}

template<typename InputType, typename OutputType>
MeanPoolingType<InputType, OutputType>::MeanPoolingType(
    const size_t kernelWidth,
    const size_t kernelHeight,
    const size_t strideWidth,
    const size_t strideHeight,
    const bool floor) :
    kernelWidth(kernelWidth),
    kernelHeight(kernelHeight),
    strideWidth(strideWidth),
    strideHeight(strideHeight),
    floor(floor),
    inSize(0),
    outSize(0),
    inputWidth(0),
    inputHeight(0),
    outputWidth(0),
    outputHeight(0),
    reset(false),
    deterministic(false),
    offset(0),
    batchSize(0)
{
  // Nothing to do here.
}

template<typename InputType, typename OutputType>
void MeanPoolingType<InputType, OutputType>::Forward(
    const InputType& input, OutputType& output)
{
  batchSize = input.n_cols;
  inSize = input.n_elem / (inputWidth * inputHeight * batchSize);
  inputTemp = arma::Cube<typename InputType::elem_type>(
      const_cast<InputType&>(input).memptr(), inputWidth, inputHeight,
      batchSize * inSize, false, false);

  if (floor)
  {
    outputWidth = std::floor((inputWidth -
        (double) kernelWidth) / (double) strideWidth + 1);
    outputHeight = std::floor((inputHeight -
        (double) kernelHeight) / (double) strideHeight + 1);

    offset = 0;
  }
  else
  {
    outputWidth = std::ceil((inputWidth -
        (double) kernelWidth) / (double) strideWidth + 1);
    outputHeight = std::ceil((inputHeight -
        (double) kernelHeight) / (double) strideHeight + 1);

    offset = 1;
  }

  outputTemp = arma::zeros<arma::Cube<typename OutputType::elem_type>>(
      outputWidth, outputHeight, batchSize * inSize);

  for (size_t s = 0; s < inputTemp.n_slices; s++)
    Pooling(inputTemp.slice(s), outputTemp.slice(s));

  output = OutputType(outputTemp.memptr(), outputTemp.n_elem / batchSize,
      batchSize);

  outputWidth = outputTemp.n_rows;
  outputHeight = outputTemp.n_cols;
  outSize = batchSize * inSize;
}

template<typename InputType, typename OutputType>
void MeanPoolingType<InputType, OutputType>::Backward(
  const InputType& /* input */,
  const OutputType& gy,
  OutputType& g)
{
  arma::Cube<typename OutputType::elem_type> mappedError =
      arma::Cube<typename OutputType::elem_type>(((OutputType&) gy).memptr(),
      outputWidth, outputHeight, outSize, false, false);

  gTemp = arma::zeros<arma::Cube<typename InputType::elem_type>>(
      inputTemp.n_rows, inputTemp.n_cols, inputTemp.n_slices);

  for (size_t s = 0; s < mappedError.n_slices; s++)
  {
    Unpooling(inputTemp.slice(s), mappedError.slice(s), gTemp.slice(s));
  }

  g = OutputType(gTemp.memptr(), gTemp.n_elem / batchSize, batchSize);
}

template<typename InputType, typename OutputType>
template<typename Archive>
void MeanPoolingType<InputType, OutputType>::serialize(
    Archive& ar,
    const uint32_t /* version */)
{
  ar(cereal::base_class<Layer<InputType, OutputType>>(this));

  ar(CEREAL_NVP(kernelWidth));
  ar(CEREAL_NVP(kernelHeight));
  ar(CEREAL_NVP(strideWidth));
  ar(CEREAL_NVP(strideHeight));
  ar(CEREAL_NVP(batchSize));
  ar(CEREAL_NVP(floor));
  ar(CEREAL_NVP(inputWidth));
  ar(CEREAL_NVP(inputHeight));
  ar(CEREAL_NVP(outputWidth));
  ar(CEREAL_NVP(outputHeight));
}

} // namespace ann
} // namespace mlpack

#endif
