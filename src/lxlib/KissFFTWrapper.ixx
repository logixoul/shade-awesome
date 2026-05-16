module;

#include <complex>
#include <kissfft/kiss_fftnd.h>
#include <vector>

export module lxlib.KissFFTWrapper;

import lxlib.Array2D;

export namespace lx {
    template<class TComponentType>
    class KissFFTWrapper {
    public:
        using Complex = std::complex<TComponentType>;
        static lx::Array2D<Complex> fftC2C(lx::Array2D<Complex> const& in) {
            return fftImpl(in, false);
        }
        static lx::Array2D<Complex> normalize(lx::Array2D<Complex> const& in) {
            return in / static_cast<TComponentType>(in.width() * in.height());
        }
        static lx::Array2D<Complex> inverseFftC2C(lx::Array2D<Complex> const& in) {
            return fftImpl(in, true);
        }

    private:
        static lx::Array2D<Complex> fftImpl(lx::Array2D<Complex> const& in, bool backward) {
            const std::vector<int> dims = { in.height(), in.width() };
            kiss_fftnd_cfg cfg = kiss_fftnd_alloc(dims.data(), 2, backward ? 1 : 0, nullptr, nullptr);
            lx::Array2D<Complex> out(in.size(), lx::nofill());

            kiss_fftnd(cfg,
                reinterpret_cast<const kiss_fft_cpx*>(in.data()),
                reinterpret_cast<kiss_fft_cpx*>(out.data()));

            kiss_fft_free(cfg);
            return out;
        }
    };
}
