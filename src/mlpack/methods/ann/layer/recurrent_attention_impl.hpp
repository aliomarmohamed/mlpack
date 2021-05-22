/**
 * @file methods/ann/layer/recurrent_attention_impl.hpp
 * @author Marcus Edel
 *
 * Implementation of the RecurrentAttention class.
 *
 * mlpack is free software; you may redistribute it and/or modify it under the
 * terms of the 3-clause BSD license.  You should have received a copy of the
 * 3-clause BSD license along with mlpack.  If not, see
 * http://www.opensource.org/licenses/BSD-3-Clause for more information.
 */
#ifndef MLPACK_METHODS_ANN_LAYER_RECURRENT_ATTENTION_IMPL_HPP
#define MLPACK_METHODS_ANN_LAYER_RECURRENT_ATTENTION_IMPL_HPP

// In case it hasn't yet been included.
#include "recurrent_attention.hpp"

#include "../visitor/load_output_parameter_visitor.hpp"
#include "../visitor/save_output_parameter_visitor.hpp"
#include "../visitor/backward_visitor.hpp"
#include "../visitor/forward_visitor.hpp"
#include "../visitor/gradient_set_visitor.hpp"
#include "../visitor/gradient_update_visitor.hpp"
#include "../visitor/gradient_visitor.hpp"

namespace mlpack {
namespace ann /** Artificial Neural Network. */ {

template<typename InputType, typename OutputType>
RecurrentAttention<InputType, OutputType>::RecurrentAttention() :
    rho(0),
    forwardStep(0),
    backwardStep(0),
    deterministic(false)
{
  // Nothing to do.
}

template<typename InputType, typename OutputType>
template<typename RNNModuleType, typename ActionModuleType>
RecurrentAttention<InputType, OutputType>::RecurrentAttention(
    const size_t outSize,
    const RNNModuleType& rnn,
    const ActionModuleType& action,
    const size_t rho) :
    outSize(outSize),
    rnnModule(new RNNModuleType(rnn)),
    actionModule(new ActionModuleType(action)),
    rho(rho),
    forwardStep(0),
    backwardStep(0),
    deterministic(false)
{
  network.push_back(rnnModule);
  network.push_back(actionModule);
}

template<typename InputType, typename OutputType>
void RecurrentAttention<InputType, OutputType>::Forward(
    const InputType& input, OutputType& output)
{
  // Initialize the action input.
  if (initialInput.is_empty())
  {
    initialInput = arma::zeros(outSize, input.n_cols);
  }

  // Propagate through the action and recurrent module.
  for (forwardStep = 0; forwardStep < rho; ++forwardStep)
  {
    if (forwardStep == 0)
    {
      actionModule->Forward(initialInput, actionModule->OutputParameter());
    }
    else
    {
      actionModule->Forward(rnnModule->OutputParameter(),
          actionModule->OutputParameter());
    }

    // Initialize the glimpse input.
    InputType glimpseInput = arma::zeros(input.n_elem, 2);
    glimpseInput.col(0) = input;
    glimpseInput.submat(0, 1, actionModule->OutputParameter().n_elem - 1, 1) =
        actionModule.OutputParameter();

    rnnModule->Forward(glimpseInput, rnnModule->OutputParameter());

    // Save the output parameter when training the module.
    if (!deterministic)
    {
      for (size_t l = 0; l < network.size(); ++l)
      {
        // TODO: what if network[i] has a Model()?
        moduleOutputParameter.push_back(network[l]->OutputParameter());
      }
    }
  }

  output = boost::apply_visitor(outputParameterVisitor, rnnModule);

  forwardStep = 0;
  backwardStep = 0;
}

template<typename InputType, typename OutputType>
void RecurrentAttention<InputType, OutputType>::Backward(
    const InputType& /* input */,
    const OutputType& gy,
    OutputType& g)
{
  if (intermediateGradient.is_empty() && backwardStep == 0)
  {
    // Initialize the attention gradients.
    // TODO: do rnnModule or actionModule have a Model()?  We may need to
    // account for those weights too.
    size_t weights = rnnModule->Parameters().n_elem +
        actionModule->Parameters().n_elem;

    intermediateGradient = arma::zeros(weights, 1);
    attentionGradient = arma::zeros(weights, 1);

    // Initialize the action error.
    actionError = arma::zeros(actionModule->OutputParameter().n_rows,
        actionModule->OutputParameter().n_cols);
  }

  // Propagate the attention gradients.
  if (backwardStep == 0)
  {
    size_t offset = 0;
    //  TODO: what if rnnModule has a Model()?
    rnnModule->Gradient() = arma::mat(intermediateGradient.memptr() + offset,
        rnnModule->Parameters().n_rows, rnnModule->Parameters().n_cols, false,
        false);
    offset += rnnModule->Parameters().n_elem;
    actionModule->Gradient() = arma::mat(intermediateGradient.memptr() + offset,
        actionModule->Parameters().n_rows, actionModule->Parameters().n_cols,
        false, false);

    attentionGradient.zeros();
  }

  // Back-propagate through time.
  for (; backwardStep < rho; backwardStep++)
  {
    if (backwardStep == 0)
    {
      recurrentError = gy;
    }
    else
    {
      recurrentError = actionDelta;
    }

    for (size_t l = 0; l < network.size(); ++l)
    {
      // TODO: handle case where HasModelCheck is true
      network[network.size() - 1 - l] = moduleOutputParameter.back();
      moduleOutputParameter.pop_back();
    }

    if (backwardStep == (rho - 1))
    {
      actionModule->Backward(actionModule->OutputParameter(), actionError,
          actionDelta);
    }
    else
    {
      actionModule->Backward(initialInput, actionError, actionDelta);
    }

    rnnModule->Backward(rnnModule->OutputParameter(), recurrentError, rnnDelta);

    if (backwardStep == 0)
    {
      g = rnnDelta.col(1);
    }
    else
    {
      g += rnnDelta.col(1);
    }

    IntermediateGradient();
  }
}

template<typename InputType, typename OutputType>
void RecurrentAttention<InputType, OutputType>::Gradient(
    const InputType& /* input */,
    const OutputType& /* error */,
    OutputType& /* gradient */)
{
  size_t offset = 0;
  // TODO: handle case where rnnModule or actionModule have a model
  if (rnnModule->Parameters().n_elem != 0)
  {
    rnnModule->Gradient() = attentionGradient.submat(offset, 0, offset +
        rnnModule->Parameters().n_elem - 1, 0);
    offset += rnnModule->Parameters().n_elem;
  }

  if (actionModule->Parameters().n_elem != 0)
  {
    actionModule->Gradient() = attentionGradient.submat(offset, 0, offset +
        actionModule->Parameters().n_elem - 1, 0);
  }
}

template<typename InputType, typename OutputType>
template<typename Archive>
void RecurrentAttention<InputType, OutputType>::serialize(
    Archive& ar, const uint32_t /* version */)
{
  ar(cereal::base_class<Layer<InputType, OutputType>>(this));

  ar(CEREAL_NVP(rho));
  ar(CEREAL_NVP(outSize));
  ar(CEREAL_NVP(forwardStep));
  ar(CEREAL_NVP(backwardStep));

  ar(CEREAL_POINTER(rnnModule));
  ar(CEREAL_POINTER(actionModule));
}

} // namespace ann
} // namespace mlpack

#endif
