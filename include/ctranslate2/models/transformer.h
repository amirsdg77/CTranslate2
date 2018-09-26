#pragma once

// This file defines the execution engine for a Transformer model trained with OpenNMT-tf
// and exported with the python/ctranslate/convert_model.py tool.

#include <map>

#include "ctranslate2/ops/ops.h"
#include "ctranslate2/storage_view.h"

#include "model.h"

namespace ctranslate2 {
  namespace models {

    class TransformerModel : public Model
    {
    public:
      TransformerModel(const std::string& path, Device device);
      const StorageView& get_variable(const std::string& scope) const;
      std::unique_ptr<Encoder> make_encoder() const override;
      std::unique_ptr<Decoder> make_decoder() const override;
      size_t version() const;
    private:
      std::map<std::string, StorageView> _variable_index;
      size_t _version;
    };

    class ScaledEmbeddings
    {
    public:
      ScaledEmbeddings(const TransformerModel& model, const std::string& scope);
      void operator()(const StorageView& ids, StorageView& output);
    private:
      ops::Gather _gather_op;
      const StorageView& _embeddings;
      const StorageView _scale;
    };

    class PositionEncoder
    {
    public:
      void operator()(StorageView& input, size_t index = 0);
    private:
      static const StorageView& get_position_encoding(size_t depth, Device device);
    };

    class Dense
    {
    public:
      Dense(const TransformerModel& model, const std::string& scope);
      void operator()(const StorageView& input,
                      StorageView& output,
                      const StorageView* index = nullptr);
    private:
      const StorageView& _weight;
      const StorageView& _bias;
      StorageView _partial_weight;
      StorageView _partial_bias;
    };

    class LayerNorm
    {
    public:
      LayerNorm(const TransformerModel& model, const std::string& scope);
      void operator()(const StorageView& input, StorageView& output);
    private:
      ops::LayerNorm _norm_op;
      const StorageView& _beta;
      const StorageView& _gamma;
    };

    class TransformerFeedForward
    {
    public:
      TransformerFeedForward(const TransformerModel& model, const std::string& scope);
      void operator()(const StorageView& input, StorageView& output);
    private:
      LayerNorm _layer_norm;
      Dense _ff1;
      Dense _ff2;
    };

    class DotProductAttention
    {
    public:
      void operator()(const StorageView& queries,
                      const StorageView& keys,
                      const StorageView& values,
                      const StorageView* values_lengths,
                      StorageView& output);
    };

    class MultiHeadAttention
    {
    public:
      MultiHeadAttention(const TransformerModel& model,
                         const std::string& scope,
                         size_t num_heads);
      void operator()(const StorageView& queries,
                      const StorageView* memory,
                      const StorageView* memory_lengths,
                      StorageView& output,
                      StorageView* cached_keys = nullptr,
                      StorageView* cached_values = nullptr,
                      int step = -1);
    private:
      size_t _num_heads;
      std::vector<Dense> _linear;
      LayerNorm _layer_norm;
      DotProductAttention _attention;
      ops::Transpose _transpose_op;

      void split_heads(const StorageView& x, StorageView& y);
      void combine_heads(const StorageView& x, StorageView& y);
      static void cache_proj(int step, StorageView& proj, StorageView& cache);
    };

    class TransformerEncoderLayer
    {
    public:
      TransformerEncoderLayer(const TransformerModel& model, const std::string& scope);
      void operator()(const StorageView& input,
                      const StorageView& lengths,
                      StorageView& output);
    private:
      MultiHeadAttention _self_attention;
      TransformerFeedForward _ff;
    };

    class TransformerDecoderLayer
    {
    public:
      TransformerDecoderLayer(const TransformerModel& model, const std::string& scope);
      void operator()(size_t step,
                      const StorageView& input,
                      const StorageView& memory,
                      const StorageView& memory_lengths,
                      StorageView& cached_self_attn_keys,
                      StorageView& cached_self_attn_values,
                      StorageView& cached_attn_keys,
                      StorageView& cached_attn_values,
                      StorageView& output);
    private:
      MultiHeadAttention _self_attention;
      MultiHeadAttention _encoder_attention;
      TransformerFeedForward _ff;
    };

    class TransformerEncoder : public Encoder
    {
    public:
      TransformerEncoder(const TransformerModel& model, const std::string& scope);
      void encode(const StorageView& ids,
                  const StorageView& lengths,
                  StorageView& output) override;
    private:
      ScaledEmbeddings _scaled_embeddings;
      PositionEncoder _position_encoder;
      LayerNorm _output_norm;
      std::vector<TransformerEncoderLayer> _layers;
    };

    class TransformerDecoderState : public DecoderState {
    public:
      TransformerDecoderState(size_t num_layers);
      void reset(const StorageView& memory,
                 const StorageView& memory_lengths) override;
    private:
      size_t _num_layers;
    };

    class TransformerDecoder : public Decoder
    {
    public:
      TransformerDecoder(const TransformerModel& model, const std::string& scope);
      void log_probs(size_t step,
                     const StorageView& ids,
                     const StorageView& candidates,
                     StorageView& output) override;
    private:
      ScaledEmbeddings _scaled_embeddings;
      PositionEncoder _position_encoder;
      LayerNorm _output_norm;
      std::vector<TransformerDecoderLayer> _layers;
      Dense _proj;
    };

  }
}