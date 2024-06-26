// Tencent is pleased to support the open source community by making ncnn available.
//
// Copyright (C) 2021 THL A29 Limited, a Tencent company. All rights reserved.
//
// Licensed under the BSD 3-Clause License (the "License"); you may not use this file except
// in compliance with the License. You may obtain a copy of the License at
//
// https://opensource.org/licenses/BSD-3-Clause
//
// Unless required by applicable law or agreed to in writing, software distributed
// under the License is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
// CONDITIONS OF ANY KIND, either express or implied. See the License for the
// specific language governing permissions and limitations under the License.

#include "pass_level2.h"

namespace pnnx {

class F_max_pool2d : public GraphRewriterPass
{
public:
    const char* match_pattern_graph() const
    {
        return R"PNNXIR(7767517
8 7
pnnx.Input              input_0     0 1 input
pnnx.Input              input_1     0 1 kernel_size
pnnx.Input              input_2     0 1 stride
pnnx.Input              input_3     0 1 padding
pnnx.Input              input_4     0 1 dilation
prim::Constant          op_0        0 1 ceil_mode value=%ceil_mode
aten::max_pool2d        op_1        6 1 input kernel_size stride padding dilation ceil_mode out
pnnx.Output             output      1 0 out
)PNNXIR";
    }

    const char* type_str() const
    {
        return "F.max_pool2d";
    }

    void write(Operator* op, const std::map<std::string, Parameter>& captured_params) const
    {
        op->params["ceil_mode"] = captured_params.at("ceil_mode");
        op->params["return_indices"] = false;
    }
};

REGISTER_GLOBAL_PNNX_GRAPH_REWRITER_PASS(F_max_pool2d, 10)

class F_max_pool2d_2 : public GraphRewriterPass
{
public:
    const char* match_pattern_graph() const
    {
        return R"PNNXIR(7767517
8 8
pnnx.Input              input_0     0 1 input
pnnx.Input              input_1     0 1 kernel_size
pnnx.Input              input_2     0 1 stride
pnnx.Input              input_3     0 1 padding
pnnx.Input              input_4     0 1 dilation
prim::Constant          op_0        0 1 ceil_mode value=%ceil_mode
aten::max_pool2d_with_indices op_1  6 2 input kernel_size stride padding dilation ceil_mode out indices
pnnx.Output             output      2 0 out indices
)PNNXIR";
    }

    const char* type_str() const
    {
        return "F.max_pool2d";
    }

    void write(Operator* op, const std::map<std::string, Parameter>& captured_params) const
    {
        op->params["ceil_mode"] = captured_params.at("ceil_mode");
        op->params["return_indices"] = true;
    }
};

REGISTER_GLOBAL_PNNX_GRAPH_REWRITER_PASS(F_max_pool2d_2, 10)

class F_max_pool2d_3 : public GraphRewriterPass
{
public:
    const char* match_pattern_graph() const
    {
        return R"PNNXIR(7767517
3 3
pnnx.Input              input_0     0 1 input
aten::max_pool_with_indices_onnx op_1 1 2 input out indices kernel_size=%kernel_size stride=%stride padding=%padding dilation=%dilation ceil_mode=%ceil_mode n_dims_axes=* n_dims_one=* n_dims_zero=* unbatched_rank=*
pnnx.Output             output      2 0 out indices
)PNNXIR";
    }

    const char* type_str() const
    {
        return "F.max_pool2d";
    }

    void write(Operator* op, const std::map<std::string, Parameter>& captured_params) const
    {
        std::vector<int> kernel_size = captured_params.at("kernel_size").ai;
        std::vector<int> dilation = captured_params.at("dilation").ai;
        std::vector<int> stride = captured_params.at("stride").ai;
        std::vector<int> padding = captured_params.at("padding").ai;
        int ceil_mode = captured_params.at("ceil_mode").i;

        if (padding.size() == 4)
        {
            padding = {padding[0], padding[1]};
        }

        op->params["kernel_size"] = kernel_size;
        op->params["dilation"] = dilation;
        op->params["stride"] = stride;
        op->params["padding"] = padding;
        op->params["ceil_mode"] = (ceil_mode != 0);
        op->params["return_indices"] = (op->outputs.size() != 1);
    }
};

REGISTER_GLOBAL_PNNX_GRAPH_REWRITER_PASS(F_max_pool2d_3, 10)

class F_max_pool2d_onnx : public GraphRewriterPass
{
public:
    const char* match_pattern_graph() const
    {
        return R"PNNXIR(7767517
3 2
pnnx.Input              input       0 1 input
MaxPool                 op_0        1 1 input out kernel_shape=%kernel_shape strides=%strides pads=%pads dilations=%dilations ceil_mode=%ceil_mode
pnnx.Output             output      1 0 out
)PNNXIR";
    }

    const char* type_str() const
    {
        return "F.max_pool2d";
    }

    bool match(const std::map<std::string, const Operator*>& matched_operators, const std::map<std::string, Parameter>& captured_params, const std::map<std::string, Attribute>& /*captured_attrs*/) const
    {
        if (captured_params.at("kernel_shape").type != 5)
            return false;

        if (captured_params.at("kernel_shape").ai.size() != 2)
            return false;

        if (captured_params.at("strides").type != 5)
            return false;

        if (captured_params.at("strides").ai.size() != 2)
            return false;

        if (captured_params.at("dilations").type != 5)
            return false;

        if (captured_params.at("dilations").ai.size() != 2)
            return false;

        if (captured_params.at("pads").type != 5)
            return false;

        int ceil_mode = captured_params.at("ceil_mode").i;

        const std::vector<int>& pads = captured_params.at("pads").ai;
        if (pads.size() != 4)
            return false;

        if (pads[0] != pads[2] || pads[1] != pads[3])
        {
            const Operator* maxpool = matched_operators.at("op_0");
            const std::vector<int>& in_shape = maxpool->inputs[0]->shape;
            const std::vector<int>& out_shape = maxpool->outputs[0]->shape;
            if (in_shape.size() < 2 || out_shape.size() < 2)
                return false;

            const int inh = in_shape[in_shape.size() - 2];
            const int inw = in_shape[in_shape.size() - 1];
            const int outh = out_shape[out_shape.size() - 2];
            const int outw = out_shape[out_shape.size() - 1];
            const int kh = captured_params.at("kernel_shape").ai[0];
            const int kw = captured_params.at("kernel_shape").ai[1];
            const int dh = captured_params.at("dilations").ai[0];
            const int dw = captured_params.at("dilations").ai[1];
            const int sh = captured_params.at("strides").ai[0];
            const int sw = captured_params.at("strides").ai[1];

            const int keh = dh * (kh - 1) + 1;
            const int kew = dw * (kw - 1) + 1;

            const int hpad = (outh - 1) * sh + keh - inh;
            const int wpad = (outw - 1) * sw + kew - inw;

            if (ceil_mode == 0 && hpad == 0 && wpad == 0)
            {
                // useless tail padding  :D
            }
            else
            {
                return false;
            }
        }

        return true;
    }

    void write(Operator* op, const std::map<std::string, Parameter>& captured_params) const
    {
        const std::vector<int>& pads = captured_params.at("pads").ai;
        int ceil_mode = captured_params.at("ceil_mode").i;

        op->params["kernel_size"] = captured_params.at("kernel_shape");
        op->params["dilation"] = captured_params.at("dilations");
        op->params["stride"] = captured_params.at("strides");
        op->params["padding"] = {pads[0], pads[1]};
        op->params["ceil_mode"] = (ceil_mode != 0);
        op->params["return_indices"] = false;
    }
};

REGISTER_GLOBAL_PNNX_GRAPH_REWRITER_PASS(F_max_pool2d_onnx, 10)

class F_max_pool2d_onnx_0 : public F_max_pool2d_onnx
{
public:
    const char* match_pattern_graph() const
    {
        return R"PNNXIR(7767517
3 2
pnnx.Input              input       0 1 input
MaxPool                 op_0        1 1 input out kernel_shape=%kernel_shape strides=%strides pads=%pads dilations=%dilations ceil_mode=%ceil_mode auto_pad=NOTSET storage_order=*
pnnx.Output             output      1 0 out
)PNNXIR";
    }
};

REGISTER_GLOBAL_PNNX_GRAPH_REWRITER_PASS(F_max_pool2d_onnx_0, 10)

class F_max_pool2d_onnx_1 : public GraphRewriterPass
{
public:
    const char* match_pattern_graph() const
    {
        return R"PNNXIR(7767517
3 2
pnnx.Input              input       0 1 input
MaxPool                 op_0        1 1 input out kernel_shape=%kernel_shape strides=%strides pads=%pads ceil_mode=%ceil_mode
pnnx.Output             output      1 0 out
)PNNXIR";
    }

    const char* type_str() const
    {
        return "F.max_pool2d";
    }

    bool match(const std::map<std::string, const Operator*>& matched_operators, const std::map<std::string, Parameter>& captured_params, const std::map<std::string, Attribute>& /*captured_attrs*/) const
    {
        if (captured_params.at("kernel_shape").type != 5)
            return false;

        if (captured_params.at("kernel_shape").ai.size() != 2)
            return false;

        if (captured_params.at("strides").type != 5)
            return false;

        if (captured_params.at("strides").ai.size() != 2)
            return false;

        if (captured_params.at("pads").type != 5)
            return false;

        int ceil_mode = captured_params.at("ceil_mode").i;

        const std::vector<int>& pads = captured_params.at("pads").ai;
        if (pads.size() != 4)
            return false;

        if (pads[0] != pads[2] || pads[1] != pads[3])
        {
            const Operator* maxpool = matched_operators.at("op_0");
            const std::vector<int>& in_shape = maxpool->inputs[0]->shape;
            const std::vector<int>& out_shape = maxpool->outputs[0]->shape;
            if (in_shape.size() < 2 || out_shape.size() < 2)
                return false;

            const int inh = in_shape[in_shape.size() - 2];
            const int inw = in_shape[in_shape.size() - 1];
            const int outh = out_shape[out_shape.size() - 2];
            const int outw = out_shape[out_shape.size() - 1];
            const int kh = captured_params.at("kernel_shape").ai[0];
            const int kw = captured_params.at("kernel_shape").ai[1];
            const int sh = captured_params.at("strides").ai[0];
            const int sw = captured_params.at("strides").ai[1];

            const int hpad = (outh - 1) * sh + kh - inh;
            const int wpad = (outw - 1) * sw + kw - inw;

            if (ceil_mode == 0 && hpad == 0 && wpad == 0)
            {
                // useless tail padding  :D
            }
            else
            {
                return false;
            }
        }

        return true;
    }

    void write(Operator* op, const std::map<std::string, Parameter>& captured_params) const
    {
        const std::vector<int>& pads = captured_params.at("pads").ai;
        int ceil_mode = captured_params.at("ceil_mode").i;

        op->params["kernel_size"] = captured_params.at("kernel_shape");
        op->params["dilation"] = {1, 1};
        op->params["stride"] = captured_params.at("strides");
        op->params["padding"] = {pads[0], pads[1]};
        op->params["ceil_mode"] = (ceil_mode != 0);
        op->params["return_indices"] = false;
    }
};

REGISTER_GLOBAL_PNNX_GRAPH_REWRITER_PASS(F_max_pool2d_onnx_1, 10)

class F_max_pool2d_onnx_2 : public GraphRewriterPass
{
public:
    const char* match_pattern_graph() const
    {
        return R"PNNXIR(7767517
3 2
pnnx.Input              input       0 1 input
MaxPool                 op_0        1 1 input out kernel_shape=%kernel_shape strides=%strides pads=%pads
pnnx.Output             output      1 0 out
)PNNXIR";
    }

    const char* type_str() const
    {
        return "F.max_pool2d";
    }

    bool match(const std::map<std::string, const Operator*>& matched_operators, const std::map<std::string, Parameter>& captured_params, const std::map<std::string, Attribute>& /*captured_attrs*/) const
    {
        if (captured_params.at("kernel_shape").type != 5)
            return false;

        if (captured_params.at("kernel_shape").ai.size() != 2)
            return false;

        if (captured_params.at("strides").type != 5)
            return false;

        if (captured_params.at("strides").ai.size() != 2)
            return false;

        if (captured_params.at("pads").type != 5)
            return false;

        const std::vector<int>& pads = captured_params.at("pads").ai;
        if (pads.size() != 4)
            return false;

        if (pads[0] != pads[2] || pads[1] != pads[3])
        {
            const Operator* maxpool = matched_operators.at("op_0");
            const std::vector<int>& in_shape = maxpool->inputs[0]->shape;
            const std::vector<int>& out_shape = maxpool->outputs[0]->shape;
            if (in_shape.size() < 2 || out_shape.size() < 2)
                return false;

            const int inh = in_shape[in_shape.size() - 2];
            const int inw = in_shape[in_shape.size() - 1];
            const int outh = out_shape[out_shape.size() - 2];
            const int outw = out_shape[out_shape.size() - 1];
            const int kh = captured_params.at("kernel_shape").ai[0];
            const int kw = captured_params.at("kernel_shape").ai[1];
            const int sh = captured_params.at("strides").ai[0];
            const int sw = captured_params.at("strides").ai[1];

            const int hpad = (outh - 1) * sh + kh - inh;
            const int wpad = (outw - 1) * sw + kw - inw;

            if (hpad == 0 && wpad == 0)
            {
                // useless tail padding  :D
            }
            else
            {
                return false;
            }
        }

        return true;
    }

    void write(Operator* op, const std::map<std::string, Parameter>& captured_params) const
    {
        const std::vector<int>& pads = captured_params.at("pads").ai;

        op->params["kernel_size"] = captured_params.at("kernel_shape");
        op->params["dilation"] = {1, 1};
        op->params["stride"] = captured_params.at("strides");
        op->params["padding"] = {pads[0], pads[1]};
        op->params["ceil_mode"] = false;
        op->params["return_indices"] = false;
    }
};

REGISTER_GLOBAL_PNNX_GRAPH_REWRITER_PASS(F_max_pool2d_onnx_2, 10)

} // namespace pnnx
