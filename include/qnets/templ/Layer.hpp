#ifndef QNETS_TEMPL_LAYER_HPP
#define QNETS_TEMPL_LAYER_HPP

#include "qnets/templ/DerivConfig.hpp"

#include <type_traits>

namespace templ
{
// --- TemplNet Layers

// Layer Config
//
// To pass non-input layer configurations as variadic parameter pack
template <typename SizeT, SizeT N_IN, SizeT N_OUT, class ACTFType>
struct LayerConfig
{
    static constexpr SizeT ninput = N_IN;
    static constexpr SizeT noutput = N_OUT;
    static constexpr SizeT nbeta = (N_IN + 1)*N_OUT;
    static constexpr SizeT nlink = N_IN*N_OUT;
    using ACTF_Type = ACTFType;

    static constexpr SizeT size() { return noutput; }
};


// The actual Layer class
//
template <typename SizeT, typename ValueT, SizeT NET_NINPUT, SizeT N_IN, SizeT N_OUT, class ACTFType, DerivConfig DCONF>
class Layer: public LayerConfig<SizeT, N_IN, N_OUT, ACTFType>
{
public: // sizes
    static constexpr StaticDFlags<DCONF> dconf{};
    static constexpr SizeT nd1 = dconf.d1 ? NET_NINPUT*N_OUT : 0; // number of input derivative values
    static constexpr SizeT nd1_feed = dconf.d1 ? NET_NINPUT*N_IN : 0; // number of deriv values from previous layer
    static constexpr SizeT nd2 = dconf.d2 ? nd1 : 0;
    static constexpr SizeT nd2_feed = dconf.d2 ? nd1_feed : 0;

private: // arrays
    std::array<ValueT, N_OUT> _out;
    std::array<ValueT, nd1> _d1;
    std::array<ValueT, nd2> _d2;
    std::array<ValueT, dconf.d1 ? N_OUT : 0> _ad1; // activation function d1
    std::array<ValueT, dconf.d2 ? N_OUT : 0> _ad2; // activation function d2

public: // public member variables
    ACTFType actf{}; // the activation function
    std::array<ValueT, (N_IN + 1)*N_OUT> beta; // the weights

    // public output read references
    const decltype(_out) &out = _out;
    const decltype(_d1) &d1 = _d1;
    const decltype(_d2) &d2 = _d2;
    //std::array<ValueT, LayerConf::nvp> vd1;

private: // private methods
    constexpr void _computeFeed(const std::array<ValueT, N_IN> &input)
    {
        for (SizeT i = 0; i < N_OUT; ++i) {
            const SizeT offset = 1 + i*(N_IN + 1);
            _out[i] = beta[offset - 1]; // bias weight
            for (SizeT j = 0; j < N_IN; ++j) {
                _out[i] += beta[offset + j]*input[j];
            }
        }
    }

    constexpr void _computeActivation(bool flag_ad1, bool flag_ad2 /*is overriding*/)
    {
        if (flag_ad2) {
            actf.fd12(_out.begin(), _out.end(), _out.begin(), _ad1.begin(), _ad2.begin());
        }
        else if (flag_ad1) {
            actf.fd1(_out.begin(), _out.end(), _out.begin(), _ad1.begin());
        }
        else {
            actf.f(_out.begin(), _out.end(), _out.begin());
        }
    }

    constexpr void _computeOutput(const std::array<ValueT, N_IN> &input, DynamicDFlags dflags)
    {
        this->_computeFeed(input);
        this->_computeActivation(dflags.d1(), dflags.d2());
    }

    constexpr void _computeD1(const std::array<ValueT, nd1_feed> &in_d1)
    {
        for (SizeT i = 0; i < N_OUT; ++i) {
            const SizeT offset = 1 + i*(N_IN + 1);
            _d1[i] = 0.;
            for (SizeT j = 0; j < N_IN; ++j) {
                _d1[i] += beta[offset + j]*in_d1[j];
            }
            _d1[i] *= _ad1[i];
        }
    }

    constexpr void _computeD12(const std::array<ValueT, nd1_feed> &in_d1, const std::array<ValueT, nd2_feed> &in_d2)
    {
        for (SizeT i = 0; i < N_OUT; ++i) {
            const SizeT offset = 1 + i*(N_IN + 1);
            _d1[i] = 0.;
            _d2[i] = 0.;
            for (SizeT j = 0; j < N_IN; ++j) {
                _d1[i] += beta[offset + j]*in_d1[j];
                _d2[i] += beta[offset + j]*in_d2[j];
            }
            _d2[i] = _ad1[i]*_d2[i] + _ad2[i]*_d1[i]*_d1[i];
            _d1[i] *= _ad1[i];
        }
    }

public: // public methods
    constexpr void PropagateInput(const std::array<ValueT, N_IN> &input, DynamicDFlags dflags) // propagation of input data (not layer)
    {
        dflags = dflags.AND(dconf); // AND static and dynamic conf
        this->_computeOutput(input, dflags);

        // fill diagonal d1,d2
        if (dflags.d1()) {
            for (SizeT i = 0; i < nd1; i += NET_NINPUT + 1) {
                _d1[i] = _ad1[i]*beta[i];
            }
        }
        if (dflags.d2()) {
            for (SizeT i = 0; i < nd2; i += NET_NINPUT + 1) {
                _d2[i] = _ad2[i]*beta[i]*beta[i];
            }
        }
    }

    constexpr void PropagateLayer(const std::array<ValueT, N_IN> &input, const std::array<ValueT, nd1_feed> &in_d1, const std::array<ValueT, nd2_feed> &in_d2, DynamicDFlags dflags)
    {
        dflags = dflags.AND(dconf); // AND static and dynamic conf
        this->_computeOutput(input, dflags);

        /*
        // first derivative
        if (_v1d != nullptr) {
            for (int i = 0; i < _nx0; ++i) {
                _v1d[i] = _a1d*_first_der[i];
            }
        }
        // second derivative
        if (_v2d != nullptr) {
            for (int i = 0; i < _nx0; ++i) {
                _v2d[i] = _a1d*_second_der[i] + _a2d*_first_der[i]*_first_der[i];
            }
        }
        */
    }
};
} // templ

#endif
