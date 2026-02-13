// Copyright Dan Price. All rights reserved.

#pragma once

namespace json2wav::math::matrix
{
	template<size_t n, typename T> class SquareMatrix;
	template<size_t n, typename T> class DiagonalMatrix;
}

template<size_t n, typename T>
inline json2wav::math::matrix::SquareMatrix<n, T> operator*(const json2wav::math::matrix::SquareMatrix<n, T>& lhs, const json2wav::math::matrix::SquareMatrix<n, T>& rhs);

template<size_t n, typename T>
inline json2wav::math::matrix::SquareMatrix<n, T> operator*(const json2wav::math::matrix::SquareMatrix<n, T>& lhs, const json2wav::math::matrix::DiagonalMatrix<n, T>& rhs);

namespace json2wav::math
{
	namespace f64
	{
		constexpr const double sqrt2  = 1.4142135623730950488;
		constexpr const double sq2inv = 1.0/sqrt2;
		constexpr const double pi     = 3.1415926535897932385;
		constexpr const double tau    = 2.0*pi;
	}
	namespace f32
	{
		constexpr const float sqrt2  = static_cast<float>(f64::sqrt2);
		constexpr const float sq2inv = static_cast<float>(f64::sq2inv);
		constexpr const float pi     = static_cast<float>(f64::pi);
		constexpr const float tau    = static_cast<float>(f64::tau);
	}

	namespace matrix
	{
		template<size_t n, typename T> class SquareMatrix;
		template<size_t n, typename T> class DiagonalMatrix;
		template<size_t n, typename T> class VerticalVector;
		template<size_t n, typename T> class HorizontalVector;
		template<size_t n>             class HadamardMatrix;

		template<typename T> inline constexpr T GetTolerance();
		template<> inline constexpr float GetTolerance<float>() { return 0.0001f; }
		template<> inline constexpr double GetTolerance<double>() { return 0.00000001; }
		template<> inline constexpr long double GetTolerance<long double>() { return 0.0000000001L; }

		template<size_t n, typename T>
		class SquareMatrix
		{
		public:
			typedef T row_t[n];
			typedef const T const_row_t[n];

			SquareMatrix()
			{
				for (size_t i = 0; i < n; ++i)
					for (size_t j = 0; j < n; ++j)
						data[i][j] = 0;
			}

			SquareMatrix(const SquareMatrix& other)
			{
				for (size_t i = 0; i < n; ++i)
					for (size_t j = 0; j < n; ++j)
						data[i][j] = other.data[i][j];
			}

			SquareMatrix(SquareMatrix&& other) noexcept
			{
				for (size_t i = 0; i < n; ++i)
					for (size_t j = 0; j < n; ++j)
						data[i][j] = other.data[i][j];
			}

			SquareMatrix& operator=(const SquareMatrix& other)
			{
				if (&other != this)
					for (size_t i = 0; i < n; ++i)
						for (size_t j = 0; j < n; ++j)
							data[i][j] = other.data[i][j];
				return *this;
			}

			SquareMatrix& operator=(SquareMatrix&& other) noexcept
			{
				if (&other != this)
					for (size_t i = 0; i < n; ++i)
						for (size_t j = 0; j < n; ++j)
							data[i][j] = other.data[i][j];
				return *this;
			}

			inline SquareMatrix(const DiagonalMatrix<n, T>& other);
			inline SquareMatrix& operator=(const DiagonalMatrix<n, T>& other);
			inline SquareMatrix& operator+=(const DiagonalMatrix<n, T>& other);
			inline SquareMatrix& operator-=(const DiagonalMatrix<n, T>& other);

			//T (& operator[](size_t i))[n]
			row_t& operator[](size_t i)
			{
				return data[i];
			}

			//const T (& operator[](size_t i) const)[n]
			const_row_t& operator[](size_t i) const
			{
				return data[i];
			}

			SquareMatrix& operator+=(const SquareMatrix& rhs)
			{
				for (size_t i; i < n; ++i)
					for (size_t j; j < n; ++j)
						data[i][j] += rhs.data[i][j];
			}

			SquareMatrix& operator-=(const SquareMatrix& rhs)
			{
				for (size_t i; i < n; ++i)
					for (size_t j; j < n; ++j)
						data[i][j] -= rhs.data[i][j];
			}

			SquareMatrix& operator*=(const SquareMatrix& rhs)
			{
				//return *this=*this*rhs; // Bahaha
				//return *this=rhs**this; // Bahahahahahaha
				const SquareMatrix& lhs = *this;
				*this = lhs*rhs;
				return *this;
			}

			SquareMatrix& operator*=(const DiagonalMatrix<n, T>& rhs)
			{
				//return *this=*this*rhs; // Bahaha
				//return *this=rhs**this; // Bahahahahahaha
				const SquareMatrix& lhs = *this;
				*this = lhs*rhs;
				return *this;
			}

			SquareMatrix GetTranspose() const
			{
				SquareMatrix transpose;
				for (size_t i = 0; i < n; ++i)
					for (size_t j = 0; j < n; ++j)
						transpose[j][i] = data[i][j];
				return transpose;
			}

			bool IsOrthogonal() const
			{
				SquareMatrix product = GetTranspose();
				product *= *this;
				const T mag = product[0][0];
				for (size_t i = 0; i < n; ++i)
					for (size_t j = 0; j < n; ++j)
						if (std::abs(product[i][j] - static_cast<T>(static_cast<int>(i == j))*mag) >= GetTolerance<T>())
							return false;
				return true;
			}

			bool IsOrthonormal() const
			{
				SquareMatrix product = GetTranspose();
				product *= *this;
				for (size_t i = 0; i < n; ++i)
					for (size_t j = 0; j < n; ++j)
						if (std::abs(product[i][j] - static_cast<T>(static_cast<int>(i == j))) >= GetTolerance<T>())
							return false;
				return true;
			}

			/*bool IsOrthonormal() const
			{
				if (!IsOrthogonal())
					return false;
				for (size_t i = 0; i < n; ++i)
				{
					T magsq = static_cast<T>(0);
					for (size_t j = 0; j < n; ++j)
						magsq += data[i][j]*data[i][j];
					if (std::abs(std::sqrt(magsq) - static_cast<T>(1)) >= GetTolerance<T>())
						return false;
				}
				return true;
			}*/

		private:
			T data[n][n];
		};

		template<size_t n, typename T, typename D>
		class Array
		{
		public:
			Array() : data{ 0 } {}

			Array(const Array& other)
			{
				for (size_t i = 0; i < n; ++i)
					data[i] = other.data[i];
			}

			Array(Array&& other) noexcept
			{
				for (size_t i = 0; i < n; ++i)
					data[i] = other.data[i];
			}

			D& operator=(const Array& other)
			{
				if (&other != this)
					for (size_t i = 0; i < n; ++i)
						data[i] = other.data[i];
				return static_cast<D&>(*this);
			}

			D& operator=(Array&& other) noexcept
			{
				if (&other != this)
					for (size_t i = 0; i < n; ++i)
						data[i] = other.data[i];
				return static_cast<D&>(*this);
			}

			D& operator+=(const Array& other)
			{
				for (size_t i = 0; i < n; ++i)
					data[i] += other.data[i];
				return static_cast<D&>(*this);
			}

			D& operator-=(const Array& other)
			{
				for (size_t i = 0; i < n; ++i)
					data[i] -= other.data[i];
				return static_cast<D&>(*this);
			}

			T& operator[](size_t i)
			{
				return data[i];
			}

			const T& operator[](size_t i) const
			{
				return data[i];
			}

		private:
			T data[n];
		};

		template<size_t n, typename T>
		class DiagonalMatrix : public Array<n, T, DiagonalMatrix<n, T>>
		{
		public:
			operator SquareMatrix<n, T>()
			{
				SquareMatrix<n, T> converted(*this);
				return converted;
			}

		};

		template<size_t n, typename T>
		class VerticalVector : public Array<n, T, VerticalVector<n, T>>
		{
		public:
		};

		template<size_t n, typename T>
		class HorizontalVector : public Array<n, T, HorizontalVector<n, T>>
		{
		public:
		};

		template<size_t n> class HadamardMatrix
		{
		private:
			template<typename T> struct op { static T times(const T& v); };

		public:
			template<typename T> static inline T array_multiply(const T& v) { return op<T>::times(v); }
		};

		template<>
		template<typename T>
		struct HadamardMatrix<2>::op
		{
			static T times(const T& v)
			{
				// [ 1,  1 ]  \ /  [ a ]  ----  [ a + b ]
				// [ 1, -1 ]  / \  [ b ]  ----  [ a - b ]
				T r;
				r[0] = v[0]+v[1];
				r[1] = v[0]-v[1];
				return r;
			}
		};

		template<>
		template<typename T>
		struct HadamardMatrix<4>::op
		{
			static T times(const T& v)
			{
				// [ 1,  1,  1,  1 ]       [ a ]        [ a + b + c + d ]
				// [ 1, -1,  1, -1 ]  \ /  [ b ]  ----  [ a - b + c - d ]
				// [ 1,  1, -1, -1 ]  / \  [ c ]  ----  [ a + b - c - d ]
				// [ 1, -1, -1,  1 ]       [ d ]        [ a - b - c + d ]
				T r;
				r[0] = (v[0]+v[1])+(v[2]+v[3]);
				r[1] = (v[0]-v[1])+(v[2]-v[3]);
				r[2] = (v[0]+v[1])-(v[2]+v[3]);
				r[3] = (v[0]-v[1])-(v[2]-v[3]);
				return r;
			}
		};

		template<>
		template<typename T>
		struct HadamardMatrix<8>::op
		{
			static T times(const T& v)
			{
				// [ 1,  1,  1,  1,  1,  1,  1,  1 ]       [ a ]        [ a + b + c + d + e + f + g + h ]
				// [ 1, -1,  1, -1,  1, -1,  1, -1 ]       [ b ]        [ a - b + c - d + e - f + g - h ]
				// [ 1,  1, -1, -1,  1,  1, -1, -1 ]       [ c ]        [ a + b - c - d + e + f - g - h ]
				// [ 1, -1, -1,  1,  1, -1, -1,  1 ]  \ /  [ d ]  ----  [ a - b - c + d + e - f - g + h ]
				// [ 1,  1,  1,  1, -1, -1, -1, -1 ]  / \  [ e ]  ----  [ a + b + c + d - e - f - g - h ]
				// [ 1, -1,  1, -1, -1,  1, -1,  1 ]       [ f ]        [ a - b + c - d - e + f - g + h ]
				// [ 1,  1, -1, -1, -1, -1,  1,  1 ]       [ g ]        [ a + b - c - d - e - f + g + h ]
				// [ 1, -1, -1,  1, -1,  1,  1, -1 ]       [ h ]        [ a - b - c + d - e + f + g - h ]
				T r;
				r[0] = ((v[0]+v[1])+(v[2]+v[3]))+((v[4]+v[5])+(v[6]+v[7]));
				r[1] = ((v[0]-v[1])+(v[2]-v[3]))+((v[4]-v[5])+(v[6]-v[7]));
				r[2] = ((v[0]+v[1])-(v[2]+v[3]))+((v[4]+v[5])-(v[6]+v[7]));
				r[3] = ((v[0]-v[1])-(v[2]-v[3]))+((v[4]-v[5])-(v[6]-v[7]));
				r[4] = ((v[0]+v[1])+(v[2]+v[3]))-((v[4]+v[5])+(v[6]+v[7]));
				r[5] = ((v[0]-v[1])+(v[2]-v[3]))-((v[4]-v[5])+(v[6]-v[7]));
				r[6] = ((v[0]+v[1])-(v[2]+v[3]))-((v[4]+v[5])-(v[6]+v[7]));
				r[7] = ((v[0]-v[1])-(v[2]-v[3]))-((v[4]-v[5])-(v[6]-v[7]));
				return r;
			}
		};

		template<>
		template<typename T>
		struct HadamardMatrix<16>::op
		{
			static T times(const T& v)
			{
				T r;
r[0]  = (((v[0]+v[1])+(v[2]+v[3]))+((v[4]+v[5])+(v[6]+v[7])))+(((v[8]+v[9])+(v[10]+v[11]))+((v[12]+v[13])+(v[14]+v[15])));
r[1]  = (((v[0]-v[1])+(v[2]-v[3]))+((v[4]-v[5])+(v[6]-v[7])))+(((v[8]-v[9])+(v[10]-v[11]))+((v[12]-v[13])+(v[14]-v[15])));
r[2]  = (((v[0]+v[1])-(v[2]+v[3]))+((v[4]+v[5])-(v[6]+v[7])))+(((v[8]+v[9])-(v[10]+v[11]))+((v[12]+v[13])-(v[14]+v[15])));
r[3]  = (((v[0]-v[1])-(v[2]-v[3]))+((v[4]-v[5])-(v[6]-v[7])))+(((v[8]-v[9])-(v[10]-v[11]))+((v[12]-v[13])-(v[14]-v[15])));
r[4]  = (((v[0]+v[1])+(v[2]+v[3]))-((v[4]+v[5])+(v[6]+v[7])))+(((v[8]+v[9])+(v[10]+v[11]))-((v[12]+v[13])+(v[14]+v[15])));
r[5]  = (((v[0]-v[1])+(v[2]-v[3]))-((v[4]-v[5])+(v[6]-v[7])))+(((v[8]-v[9])+(v[10]-v[11]))-((v[12]-v[13])+(v[14]-v[15])));
r[6]  = (((v[0]+v[1])-(v[2]+v[3]))-((v[4]+v[5])-(v[6]+v[7])))+(((v[8]+v[9])-(v[10]+v[11]))-((v[12]+v[13])-(v[14]+v[15])));
r[7]  = (((v[0]-v[1])-(v[2]-v[3]))-((v[4]-v[5])-(v[6]-v[7])))+(((v[8]-v[9])-(v[10]-v[11]))-((v[12]-v[13])-(v[14]-v[15])));
r[8]  = (((v[0]+v[1])+(v[2]+v[3]))+((v[4]+v[5])+(v[6]+v[7])))-(((v[8]+v[9])+(v[10]+v[11]))+((v[12]+v[13])+(v[14]+v[15])));
r[9]  = (((v[0]-v[1])+(v[2]-v[3]))+((v[4]-v[5])+(v[6]-v[7])))-(((v[8]-v[9])+(v[10]-v[11]))+((v[12]-v[13])+(v[14]-v[15])));
r[10] = (((v[0]+v[1])-(v[2]+v[3]))+((v[4]+v[5])-(v[6]+v[7])))-(((v[8]+v[9])-(v[10]+v[11]))+((v[12]+v[13])-(v[14]+v[15])));
r[11] = (((v[0]-v[1])-(v[2]-v[3]))+((v[4]-v[5])-(v[6]-v[7])))-(((v[8]-v[9])-(v[10]-v[11]))+((v[12]-v[13])-(v[14]-v[15])));
r[12] = (((v[0]+v[1])+(v[2]+v[3]))-((v[4]+v[5])+(v[6]+v[7])))-(((v[8]+v[9])+(v[10]+v[11]))-((v[12]+v[13])+(v[14]+v[15])));
r[13] = (((v[0]-v[1])+(v[2]-v[3]))-((v[4]-v[5])+(v[6]-v[7])))-(((v[8]-v[9])+(v[10]-v[11]))-((v[12]-v[13])+(v[14]-v[15])));
r[14] = (((v[0]+v[1])-(v[2]+v[3]))-((v[4]+v[5])-(v[6]+v[7])))-(((v[8]+v[9])-(v[10]+v[11]))-((v[12]+v[13])-(v[14]+v[15])));
r[15] = (((v[0]-v[1])-(v[2]-v[3]))-((v[4]-v[5])-(v[6]-v[7])))-(((v[8]-v[9])-(v[10]-v[11]))-((v[12]-v[13])-(v[14]-v[15])));
				return r;
			}
		};

		class InvalidShuffleMatrix
		{
		};

		template<size_t n>
		class ShuffleMatrix
		{
		public:
			ShuffleMatrix(const size_t (&shuffleInit)[n], const bool (&invertInit)[n])
			{
				bool check[n];
				for (size_t i = 0; i < n; ++i)
					check[i] = false;
				for (size_t i = 0; i < n; ++i)
				{
					if (shuffleInit[i] >= n || check[shuffleInit[i]])
						throw InvalidShuffleMatrix();
					check[shuffleInit[i]] = true;
					shuffle[i] = shuffleInit[i];
					invert[i] = invertInit[i];
				}
			}

			template<typename T>
			T multiply_from_lhs(const T& v) const
			{
				// shuffle = [ 1, 2, 0, 3 ]
				// invert  = [ 0, 1, 0, 1 ]
				//
				// [ 0,  1,  0,  0 ]       [ a ]        [  b ]
				// [ 0,  0, -1,  0 ]  \ /  [ b ]  ----  [ -c ]
				// [ 1,  0,  0,  0 ]  / \  [ c ]  ----  [  a ]
				// [ 0,  0,  0, -1 ]       [ d ]        [ -d ]
				T r;
				for (size_t i = 0; i < n; ++i)
				{
					const auto val = v[shuffle[i]];
					decltype(val) nobranch[2] = { val, -val };
					r[i] = nobranch[invert[i]];
				}
				return r;
			}

			template<typename T>
			T multiply_from_rhs(const T& v) const
			{
				// shuffle = [ 1, 2, 0, 3 ]
				// invert  = [ 0, 1, 0, 1 ]
				//
				//                    [ 0,  1,  0,  0 ]
				// [ a, b, c, d ]  *  [ 0,  0, -1,  0 ]  =  [ c, a, -b, -d ]
				//                    [ 1,  0,  0,  0 ]
				//                    [ 0,  0,  0, -1 ]
				T r;
				for (size_t i = 0; i < n; ++i)
				{
					const auto val = v[i];
					decltype(val) nobranch[2] = { val, -val };
					r[shuffle[i]] = nobranch[invert[i]];
				}
				return r;
			}

			template<typename T>
			SquareMatrix<n, T> ToSquareMatrix() const
			{
				SquareMatrix<n, T> sm;
				for (size_t i = 0; i < n; ++i)
				{
					for (size_t j = 0; j < n; ++j)
					{
						if (j == shuffle[i])
						{
							const T val = 1;
							const T nobranch[2] = { val, -val };
							sm[i][j] = nobranch[invert[i]];
						}
						else
						{
							sm[i][j] = static_cast<T>(0);
						}
					}
				}
				return sm;
			}

		private:
			size_t shuffle[n];
			bool invert[n];
		};

		template<size_t n, typename T>
		inline SquareMatrix<n, T>::SquareMatrix(const DiagonalMatrix<n, T>& other)
		{
			for (size_t i = 0; i < n; ++i)
				for (size_t j = 0; j < n; ++j)
					data[i][j] = (i == j) ? other[i] : 0;
		}

		template<size_t n, typename T>
		inline SquareMatrix<n, T>& SquareMatrix<n, T>::operator=(const DiagonalMatrix<n, T>& other)
		{
			for (size_t i = 0; i < n; ++i)
				for (size_t j = 0; j < n; ++j)
					data[i][j] = (i == j) ? other[i] : 0;
			return *this;
		}

		template<size_t n, typename T>
		inline SquareMatrix<n, T>& SquareMatrix<n, T>::operator+=(const DiagonalMatrix<n, T>& other)
		{
			for (size_t i = 0; i < n; ++i)
				data[i][i] += other[i];
			return *this;
		}

		template<size_t n, typename T>
		inline SquareMatrix<n, T>& SquareMatrix<n, T>::operator-=(const DiagonalMatrix<n, T>& other)
		{
			for (size_t i = 0; i < n; ++i)
				data[i][i] -= other[i];
			return *this;
		}

		template<size_t n, typename T>
		using StaticMatrix = T[n][n];

		template<typename T> inline const StaticMatrix<2, T>& GetHadamard2()
		{
			static const T mtx[2][2] = {
				{ 1,  1 },
				{ 1, -1 }
			};
			return mtx;
		}

		template<typename T> inline const StaticMatrix<4, T>& GetHadamard4()
		{
			static const T mtx[4][4] = {
				{ 1,  1,  1,  1 },
				{ 1, -1,  1, -1 },
				{ 1,  1, -1, -1 },
				{ 1, -1, -1,  1 }
			};
			return mtx;
		}

		template<typename T> inline const StaticMatrix<8, T>& GetHadamard8()
		{
			static const T mtx[8][8] = {
				{ 1,  1,  1,  1,  1,  1,  1,  1 },
				{ 1, -1,  1, -1,  1, -1,  1, -1 },
				{ 1,  1, -1, -1,  1,  1, -1, -1 },
				{ 1, -1, -1,  1,  1, -1, -1,  1 },
				{ 1,  1,  1,  1, -1, -1, -1, -1 },
				{ 1, -1,  1, -1, -1,  1, -1,  1 },
				{ 1,  1, -1, -1, -1, -1,  1,  1 },
				{ 1, -1, -1,  1, -1,  1,  1, -1 }
			};
			return mtx;
		}

	}
}

template<size_t n, typename T>
inline json2wav::math::matrix::SquareMatrix<n, T> operator+(const json2wav::math::matrix::SquareMatrix<n, T>& lhs, const json2wav::math::matrix::SquareMatrix<n, T>& rhs)
{
	json2wav::math::matrix::SquareMatrix result(lhs);
	result += rhs;
	return result;
}

template<size_t n, typename T>
inline json2wav::math::matrix::SquareMatrix<n, T> operator-(const json2wav::math::matrix::SquareMatrix<n, T>& lhs, const json2wav::math::matrix::SquareMatrix<n, T>& rhs)
{
	json2wav::math::matrix::SquareMatrix<n, T> result(lhs);
	result -= rhs;
	return result;
}

template<size_t n, typename T>
inline json2wav::math::matrix::DiagonalMatrix<n, T> operator+(const json2wav::math::matrix::DiagonalMatrix<n, T>& lhs, const json2wav::math::matrix::DiagonalMatrix<n, T>& rhs)
{
	json2wav::math::matrix::DiagonalMatrix result(lhs);
	result += rhs;
	return result;
}

template<size_t n, typename T>
inline json2wav::math::matrix::DiagonalMatrix<n, T> operator-(const json2wav::math::matrix::DiagonalMatrix<n, T>& lhs, const json2wav::math::matrix::DiagonalMatrix<n, T>& rhs)
{
	json2wav::math::matrix::DiagonalMatrix<n, T> result(lhs);
	result -= rhs;
	return result;
}

template<size_t n, typename T>
inline json2wav::math::matrix::VerticalVector<n, T> operator+(const json2wav::math::matrix::VerticalVector<n, T>& lhs, const json2wav::math::matrix::VerticalVector<n, T>& rhs)
{
	json2wav::math::matrix::VerticalVector<n, T> result(lhs);
	result += rhs;
	return result;
}

template<size_t n, typename T>
inline json2wav::math::matrix::VerticalVector<n, T> operator-(const json2wav::math::matrix::VerticalVector<n, T>& lhs, const json2wav::math::matrix::VerticalVector<n, T>& rhs)
{
	json2wav::math::matrix::VerticalVector<n, T> result(lhs);
	result -= rhs;
	return result;
}

template<size_t n, typename T>
inline json2wav::math::matrix::HorizontalVector<n, T> operator+(const json2wav::math::matrix::HorizontalVector<n, T>& lhs, const json2wav::math::matrix::HorizontalVector<n, T>& rhs)
{
	json2wav::math::matrix::HorizontalVector<n, T> result(lhs);
	result += rhs;
	return result;
}

template<size_t n, typename T>
inline json2wav::math::matrix::HorizontalVector<n, T> operator-(const json2wav::math::matrix::HorizontalVector<n, T>& lhs, const json2wav::math::matrix::HorizontalVector<n, T>& rhs)
{
	json2wav::math::matrix::HorizontalVector<n, T> result(lhs);
	result -= rhs;
	return result;
}

template<size_t n, typename T>
inline json2wav::math::matrix::SquareMatrix<n, T> operator*(const json2wav::math::matrix::SquareMatrix<n, T>& lhs, const json2wav::math::matrix::SquareMatrix<n, T>& rhs)
{
	json2wav::math::matrix::SquareMatrix<n, T> result;
	for (size_t i = 0; i < n; ++i)
		for (size_t j = 0; j < n; ++j)
			for (size_t k = 0; k < n; ++k)
				result[i][j] += lhs[i][k]*rhs[k][j];
	return result;
}

template<size_t n, typename T>
inline json2wav::math::matrix::SquareMatrix<n, T> operator*(const json2wav::math::matrix::DiagonalMatrix<n, T>& lhs, const json2wav::math::matrix::SquareMatrix<n, T>& rhs)
{
	json2wav::math::matrix::SquareMatrix<n, T> result;
	for (size_t i = 0; i < n; ++i)
		for (size_t j = 0; j < n; ++j)
			result[i][j] = lhs[i]*rhs[i][j];
	return result;
}

template<size_t n, typename T>
inline json2wav::math::matrix::SquareMatrix<n, T> operator*(const json2wav::math::matrix::SquareMatrix<n, T>& lhs, const json2wav::math::matrix::DiagonalMatrix<n, T>& rhs)
{
	json2wav::math::matrix::SquareMatrix<n, T> result;
	for (size_t i = 0; i < n; ++i)
		for (size_t j = 0; j < n; ++j)
			result[i][j] = lhs[i][j]*rhs[j];
	return result;
}

template<size_t n, typename T>
inline json2wav::math::matrix::DiagonalMatrix<n, T> operator*(const json2wav::math::matrix::DiagonalMatrix<n, T>& lhs, const json2wav::math::matrix::DiagonalMatrix<n, T>& rhs)
{
	json2wav::math::matrix::DiagonalMatrix<n, T> result;
	for (size_t i = 0; i < n; ++i)
		result[i] = lhs[i]*rhs[i];
	return result;
}

template<size_t n, typename T>
inline json2wav::math::matrix::VerticalVector<n, T> operator*(const json2wav::math::matrix::DiagonalMatrix<n, T>& lhs, const json2wav::math::matrix::VerticalVector<n, T>& rhs)
{
	json2wav::math::matrix::VerticalVector<n, T> result;
	for (size_t i = 0; i < n; ++i)
		result[i] = lhs[i]*rhs[i];
	return result;
}

template<size_t n, typename T>
inline json2wav::math::matrix::HorizontalVector<n, T> operator*(const json2wav::math::matrix::HorizontalVector<n, T>& lhs, const json2wav::math::matrix::DiagonalMatrix<n, T>& rhs)
{
	json2wav::math::matrix::HorizontalVector<n, T> result;
	for (size_t i = 0; i < n; ++i)
		result[i] = lhs[i]*rhs[i];
	return result;
}

template<size_t n, typename T>
inline json2wav::math::matrix::VerticalVector<n, T> operator*(const json2wav::math::matrix::SquareMatrix<n, T>& lhs, const json2wav::math::matrix::VerticalVector<n, T>& rhs)
{
	json2wav::math::matrix::VerticalVector<n, T> result;
	for (size_t i = 0; i < n; ++i)
		for (size_t k = 0; k < n; ++k)
			result[i] += lhs[i][k]*rhs[k];
	return result;
}

template<size_t n, typename T>
inline json2wav::math::matrix::HorizontalVector<n, T> operator*(const json2wav::math::matrix::HorizontalVector<n, T>& lhs, const json2wav::math::matrix::SquareMatrix<n, T>& rhs)
{
	json2wav::math::matrix::HorizontalVector<n, T> result;
	for (size_t j = 0; j < n; ++j)
		for (size_t k = 0; k < n; ++k)
			result[j] += lhs[k]*rhs[k][j];
	return result;
}

template<size_t n, typename T>
inline json2wav::math::matrix::VerticalVector<n, T> operator*(const json2wav::math::matrix::HadamardMatrix<n>& lhs, const json2wav::math::matrix::VerticalVector<n, T>& rhs)
{
	return json2wav::math::matrix::HadamardMatrix<n>::array_multiply(rhs);
}

template<size_t n, typename T>
inline json2wav::math::matrix::HorizontalVector<n, T> operator*(const json2wav::math::matrix::HorizontalVector<n, T>& lhs, const json2wav::math::matrix::HadamardMatrix<n>& rhs)
{
	return json2wav::math::matrix::HadamardMatrix<n>::array_multiply(lhs);
}

template<size_t n, typename T>
inline json2wav::math::matrix::VerticalVector<n, T> operator*(const json2wav::math::matrix::ShuffleMatrix<n>& lhs, const json2wav::math::matrix::VerticalVector<n, T>& rhs)
{
	return lhs.multiply_from_lhs(rhs);
}

template<size_t n, typename T>
inline json2wav::math::matrix::HorizontalVector<n, T> operator*(const json2wav::math::matrix::HorizontalVector<n, T>& lhs, const json2wav::math::matrix::ShuffleMatrix<n>& rhs)
{
	return rhs.multiply_from_rhs(lhs);
}

template<size_t n, typename T>
inline json2wav::math::matrix::VerticalVector<n, T> operator*(const T lhs, const json2wav::math::matrix::VerticalVector<n, T>& rhs)
{
	json2wav::math::matrix::VerticalVector<n, T> v;
	for (size_t i = 0; i < n; ++i)
		v[i] = lhs*rhs[i];
	return v;
}

template<size_t n, typename T>
inline json2wav::math::matrix::HorizontalVector<n, T> operator*(const T lhs, const json2wav::math::matrix::HorizontalVector<n, T>& rhs)
{
	json2wav::math::matrix::HorizontalVector<n, T> v;
	for (size_t i = 0; i < n; ++i)
		v[i] = lhs*rhs[i];
	return v;
}

