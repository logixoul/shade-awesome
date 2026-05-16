module;
#include "precompiled.h"


export module lxlib.Array2D_imageProc;

using namespace std;
import lxlib.Array2D;
import lxlib.stuff;

export namespace lx {
	namespace WrapModes {
	struct MirrorWrap {
		template<class T>
      static T& fetch(lx::Array2D<T>& src, int x, int y)
		{
            if (x >= src.width()) x = src.width() - (x - src.width()) - 1;
			else if (x < 0) x = -x;
            if (y >= src.height()) y = src.height() - (y - src.height()) - 1;
			else if (y < 0) y = -y;
			return src(x, y);
		}
	};
	struct Wrap {
		template<class T>
      static T& fetch(lx::Array2D<T>& src, int x, int y)
		{
			ivec2 wp(x, y);
         wp.x %= src.width(); if (wp.x < 0) wp.x += src.width();
			wp.y %= src.height(); if (wp.y < 0) wp.y += src.height();
			return src(wp);
		}
	};
	struct ZeroesOutside {
		template<class T>
      static T& fetch(lx::Array2D<T>& src, int x, int y)
		{
         if (x < 0 || y < 0 || x >= src.width() || y >= src.height())
           {
				static T zeroRef = T(0.0f);
				return zeroRef;
			}
			return src(x, y);
		}
	};
	struct NoWrap {
		template<class T>
      static T& fetch(lx::Array2D<T>& src, int x, int y)
		{
			return src(x, y);
		}
	};
	struct Clamp {
		template<class T>
      static T& fetch(lx::Array2D<T>& src, int x, int y)
		{
			if (x < 0) x = 0;
           if (x > src.width() - 1) x = src.width() - 1;
			if (y < 0) y = 0;
           if (y > src.height() - 1) y = src.height() - 1;
			
			return src(x, y);
		}
  };
	}

	template<class T, class WrapPolicy>
	void splatBilinearPoint(lx::Array2D<T>& dst, glm::vec2 const& p, T const& value)
	{
	int ix = p.x, iy = p.y;
	float fx = ix, fy = iy;
	if (p.x < 0.0f && fx != p.x) { fx--; ix--; }
	if (p.y < 0.0f && fy != p.y) { fy--; iy--; }
	float fractx = p.x - fx;
	float fracty = p.y - fy;
	float fractx1 = 1.0 - fractx;
	float fracty1 = 1.0 - fracty;
	WrapPolicy::fetch(dst, ix, iy) += (fractx1 * fracty1) * value;
	WrapPolicy::fetch(dst, ix, iy + 1) += (fractx1 * fracty) * value;
	WrapPolicy::fetch(dst, ix + 1, iy) += (fractx * fracty1) * value;
        WrapPolicy::fetch(dst, ix + 1, iy + 1) += (fractx * fracty) * value;
	}
	template<class T, class WrapPolicy>
	T getBilinear(lx::Array2D<T> const& src, glm::vec2 const& p)
	{
	int ix = p.x, iy = p.y;
	float fx = ix, fy = iy;
	if (p.x < 0.0f && fx != p.x) { fx--; ix--; }
	if (p.y < 0.0f && fy != p.y) { fy--; iy--; }
	float fractx = p.x - fx;
	float fracty = p.y - fy;
	return lerp(
		lerp(WrapPolicy::fetch(src, ix, iy), WrapPolicy::fetch(src, ix + 1, iy), fractx),
       lerp(WrapPolicy::fetch(src, ix, iy + 1), WrapPolicy::fetch(src, ix + 1, iy + 1), fractx),
		fracty);
   }

    template<class T>
	T& zero() {
	static T val = T()*0.0f;
	val = T(0.0f);
	return val;
   }

    template<class T>
	lx::Array2D<T> gauss3(lx::Array2D<T> src) {
     T zero = T(0.0f);
		lx::Array2D<T> dst1(src.width(), src.height());
		lx::Array2D<T> dst2(src.width(), src.height());
 for(auto p : dst1.coords())
		dst1(p) = .25f * (2.0f * get_clamped(src, p.x, p.y) + get_clamped(src, p.x - 1, p.y) + get_clamped(src, p.x + 1, p.y));
 for(auto p : dst2.coords())
		dst2(p) = .25f * (2.0f * get_clamped(dst1, p.x, p.y) + get_clamped(dst1, p.x, p.y - 1) + get_clamped(dst1, p.x, p.y + 1));
        return dst2;
	}

   template<class T, class WrapPolicy = lx::WrapModes::Clamp>
	vec2 gradient_i(lx::Array2D<T>& src, ivec2 const& p)
	{
	vec2 gradient;
	gradient.x = (WrapPolicy::fetch(src, p.x + 1, p.y) - WrapPolicy::fetch(src, p.x - 1, p.y)) / 2.0f;
	gradient.y = (WrapPolicy::fetch(src, p.x, p.y + 1) - WrapPolicy::fetch(src, p.x, p.y - 1)) / 2.0f;
        return gradient;
	}
template<class T, class WrapPolicy>
vec2 gradient_i_nodiv(lx::Array2D<T>& src, ivec2 const& p)
{
	vec2 gradient(
		WrapPolicy::fetch(src, p.x + 1, p.y) - WrapPolicy::fetch(src, p.x - 1, p.y),
		WrapPolicy::fetch(src, p.x, p.y + 1) - WrapPolicy::fetch(src, p.x, p.y - 1));
	return gradient;
}
  template<class T, class WrapPolicy>
	lx::Array2D<vec2> get_gradients(lx::Array2D<T>& src)
	{
	auto src2 = src.clone();
 for(auto p : src2.coords())
		src2(p) /= 2.0f;
     lx::Array2D<vec2> gradients(src.width(), src.height());
	for (int x = 0; x < src.width(); x++)
	{
		gradients(x, 0) = gradient_i_nodiv<T, WrapPolicy>(src2, ivec2(x, 0));
       gradients(x, src.height() - 1) = gradient_i_nodiv<T, WrapPolicy>(src2, ivec2(x, src.height() - 1));
	}
 for (int y = 1; y < src.height() - 1; y++)
	{
		gradients(0, y) = gradient_i_nodiv<T, WrapPolicy>(src2, ivec2(0, y));
       gradients(src.width() - 1, y) = gradient_i_nodiv<T, WrapPolicy>(src2, ivec2(src.width() - 1, y));
	}
   for (int y = 1; y < src.height() - 1; y++) {
		for (int x = 1; x < src.width() - 1; x++) {
            gradients(x, y) = gradient_i_nodiv<T, lx::WrapModes::NoWrap>(src2, ivec2(x, y));
		}
	}
       return gradients;
	}

  template<class T, class WrapPolicy>
	lx::Array2D<T> separableConvolve(lx::Array2D<T>& src, vector<float> const& kernel) {
	int ksize = kernel.size();
	int r = ksize / 2;

       T zero = lx::zero<T>();
		lx::Array2D<T> dst1(src.width(), src.height());
		lx::Array2D<T> dst2(src.width(), src.height());

   int w = src.width(), h = src.height();

	for (int y = 0; y < h; y++)
	{
		auto blurVert = [&](int x0, int x1) {
			x0 = std::max(x0, 0);
			x1 = std::min(x1, w);

			for (int x = x0; x < x1; x++)
			{
				T sum = zero;
				for (int xadd = -r; xadd <= r; xadd++)
				{
					sum += kernel[xadd + r] * (WrapPolicy::fetch(src, x + xadd, y));
				}
				dst1(x, y) = sum;
			}
		};

		blurVert(0, r);
		blurVert(w - r, w);
		for (int x = r; x < w - r; x++)
		{
			T sum = zero;
			for (int xadd = -r; xadd <= r; xadd++)
			{
				sum += kernel[xadd + r] * src(x + xadd, y);
			}
			dst1(x, y) = sum;
		}
	}

	for (int x = 0; x < w; x++)
	{
		auto blurHorz = [&](int y0, int y1) {
			y0 = std::max(y0, 0);
			y1 = std::min(y1, h);
			for (int y = y0; y < y1; y++)
			{
				T sum = zero;
				for (int yadd = -r; yadd <= r; yadd++)
				{
					sum += kernel[yadd + r] * WrapPolicy::fetch(dst1, x, y + yadd);
				}
				dst2(x, y) = sum;
			}
		};

		blurHorz(0, r);
		blurHorz(h - r, h);
		for (int y = r; y < h - r; y++)
		{
			T sum = zero;
			for (int yadd = -r; yadd <= r; yadd++)
			{
				sum += kernel[yadd + r] * dst1(x, y + yadd);
			}
			dst2(x, y) = sum;
		}
	}
        return dst2;
	}

// This is a common heuristic for choosing sigma based on kernel size, used in
// OpenCV and elsewhere.
  float sigmaFromKsize(float ksize) {
	float sigma = 0.3 * ((ksize - 1) * 0.5 - 1) + 0.8;
	return sigma;
   }

// This is the inverse of the above heuristic, which is useful for choosing
// kernel size based on a desired sigma.
  float ksizeFromSigma(float sigma) {
	int ksize = ceil(((sigma - 0.8) / 0.3 + 1) / 0.5 + 1);
	if (ksize % 2 == 0)
		ksize++;
	return ksize;
   }

    vector<float> getGaussianKernel(int ksize, float sigma) {
	vector<float> result;
	int r = ksize / 2;
	float sum = 0.0f;
	for (int i = -r; i <= r; i++) {
            float exponent = -(i * i / lx::sq(2 * sigma));
		float val = exp(exponent);
		sum += val;
		result.push_back(val);
	}
	for (int i = 0; i < result.size(); i++) {
		result[i] /= sum;
	}
      return result;
	}

  template<class T, class WrapPolicy>
	lx::Array2D<T> gaussianBlur(lx::Array2D<T> src, int ksize) {
		auto kernel = lx::getGaussianKernel(ksize, lx::sigmaFromKsize(ksize));
		return lx::separableConvolve<T, WrapPolicy>(src, kernel);
	}

   void mm(string desc, lx::Array2D<float> arr) {
	if (desc != "") {
		cout << "[" << desc << "] ";
	}
	cout << "min: " << *std::min_element(arr.begin(), arr.end()) << ", "
		<< "max: " << *std::max_element(arr.begin(), arr.end()) << endl;
   }
	void mm(string desc, lx::Array2D<vec3> arr) {
	if (desc != "") {
		cout << "[" << desc << "] ";
	}
	float const* data = (float*)arr.data();
	int const area = arr.width() * arr.height();
	cout << "min: " << *std::min_element(data, data + area * 3) << ", "
		<< "max: " << *std::max_element(data, data + area * 3) << endl;
   }
	void mm(string desc, lx::Array2D<vec2> arr) {
	if (desc != "") {
		cout << "[" << desc << "] ";
	}
	float const* data = (float*)arr.data();
	int const area = arr.width() * arr.height();
	cout << "min: " << *std::min_element(data, data + area * 2) << ", "
		<< "max: " << *std::max_element(data, data + area * 2) << endl;
}

// Linearly remaps the values in the array to be between 0 and 1, based
// on the min and max values in the array.
  lx::Array2D<float> to01(lx::Array2D<float> a) {
	auto minn = *std::min_element(a.begin(), a.end());
	auto maxx = *std::max_element(a.begin(), a.end());
	auto b = a.clone();
  for(auto p : b.coords()) {
		b(p) -= minn;
		b(p) /= (maxx - minn);
	}
       return b;
	}
}

