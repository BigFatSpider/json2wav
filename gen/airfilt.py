# Copyright Dan Price 2026.

import numpy as np
from numpy.polynomial import Polynomial as poly
import matplotlib.pyplot as plt
import scipy.signal as sig
import flerp
import t2f

_bessel2_3db = [root.real for root in poly([-9, 0, 3, 0, 1]).roots() if root.real > 0 and abs(root.imag) < 0.00000001][0]
_bessel3_3db = [root.real for root in poly([-225, 0, 45, 0, 6, 0, 1]).roots() if root.real > 0 and abs(root.imag) < 0.00000001][0]
#_bessel4_3db = 2.113917674904216
_bessel4_3db = [root.real for root in poly([-11025, 0, 1575, 0, 135, 0, 10, 0, 1]).roots() if root.real > 0 and abs(root.imag) < 0.00000001][0]

def get_bessel2(fc, sr=44100):
    fc44100 = fc if sr == 44100 else fc*44100/sr
    f0 = flerp.funwarp(fc44100)*fc44100
    z0 = flerp.f2z(f0)
    return get_bessel2_zinf(f0, sr=44100, z_inf=z0)

def get_bessel2_old(fc, sr=44100):
    wc = 2*np.pi*fc/_bessel2_3db
    k = 1/np.tan(wc/(2*sr))
    k2 = k*k
    a0 = (k + 3)*k + 3
    a1 = (-2*k2 + 6)/a0
    a2 = ((k - 3)*k + 3)/a0
    b0 = 3/a0
    b1 = 6/a0
    b2 = b0
    return [b0, b1, b2], [1.0, a1, a2]

def get_bessel2_zinf(fc, sr=44100, z_inf=-1):
    # H(s) = 3/(s^2 + 3*s + 3)
    # w0 = 2pi*fc/sr/_bessel2_3db
    # 1   <- (1 + 2z^-1 + z^-2)(1 - cosw0)
    # s   <- (1 - z^-2)sinw0
    # s^2 <- (1 - 2z^-1 + z^-2)(1 + cosw0)
    # H(z) = 3*(1 - 2*z_inf*z^-1 + z_inf^2*z^-2)*(1 - cosw0)/
    #        ((1 - 2*z^-1 + z^-2)*(1 + cosw0) + 3*(1 - z^-2)*sinw0 + 3*(1 + 2*z^-1 + z^-2)*(1 - cosw0))
    # H(z) = 3*(1 - 2*z_inf*z^-1 + z_inf^2*z^-2)*(1 - cosw0)/
    #        ((1 + cosw0 + 3*sinw0 + 3 - 3*cosw0) + z^-1*(-2 - 2*cosw0 + 6 - 6*cosw0) + z^-2*(1 + cosw0 - 3*sinw0 + 3 - 3*cosw0))
    # H(z) = 3*(1 - 2*z_inf*z^-1 + z_inf^2*z^-2)*(1 - cosw0)/
    #        ((4 - 2*cosw0 + 3*sinw0) + z^-1*(4 - 8*cosw0) + z^-2*(4 - 2*cosw0 - 3*sinw0))
    # H(z) = (3 - 6*z_inf*z^-1 + 3*z_inf^2*z^-2)*(1 - cosw0)/
    #        ((4 - 2*cosw0 + 3*sinw0) + (4 - 8*cosw0)*z^-1 + (4 - 2*cosw0 - 3*sinw0)*z^-2)
    # H(z) = ((3 - 3*cosw0) + (-6*z_inf + 6*z_inf*cosw0)*z^-1 + (3*z_inf^2 - 3*z_inf^2*cosw0)*z^-2)/
    #        ((4 - 2*cosw0 + 3*sinw0) + (4 - 8*cosw0)*z^-1 + (4 - 2*cosw0 - 3*sinw0)*z^-2)
    # a0 = 4 - 2*cosw0 + 3*sinw0
    # a1 = 4 - 8*cosw0
    # a2 = 4 - 2*cosw0 - 3*sinw0
    # b0 = 3*(1 - cosw0)*norm
    # b1 = -6*z_inf*(1 - cosw0)*norm
    # b2 = 3*z_inf^2*(1 - cosw0)*norm
    # (z_inf, abs(dc))
    # (-1, 1)
    # (-3, 4)
    # (-5, 9)
    # (-7, 16)
    # abs(dc) = ((1 - z_inf)/2)^2
    # norm = 4/(1 - z_inf)^2
    # b0 = 12*(1 - cosw0)/(1 - z_inf)^2
    # b1 = -24*z_inf*(1 - cosw0)/(1 - z_inf)^2
    # b2 = 12*z_inf^2*(1 - cosw0)/(1 - z_inf)^2

    w0 = 2.0*np.pi*fc/sr/_bessel2_3db
    eiw0 = np.exp(complex(0.0, w0))
    _2cosw0 = 2.0*eiw0.real
    _3sinw0 = 3.0*eiw0.imag
    oa0 = 1.0/(4.0 - _2cosw0 + _3sinw0)
    a1 = (4.0 - 8.0*eiw0.real)*oa0
    a2 = (4.0 - _2cosw0 - _3sinw0)*oa0
    _1mzinf = 1.0 - z_inf
    b0 = 12.0*(1.0 - eiw0.real)/(_1mzinf*_1mzinf)*oa0
    zinfb0 = z_inf*b0
    b1 = -2.0*zinfb0
    b2 = z_inf*zinfb0
    return [b0, b1, b2], [1.0, a1, a2]

def get_bessel3(fc, sr=44100):
    wc = 2*np.pi*fc/_bessel3_3db
    k = 1/np.tan(wc/(2*sr))
    a0 = ((k + 6)*k + 15)*k + 15
    a1 = (((-3*k - 6)*k + 15)*k + 45)/a0
    a2 = (((3*k - 6)*k - 15)*k + 45)/a0
    a3 = (((-k + 6)*k - 15)*k + 15)/a0
    b0 = 15/a0
    b1 = 45/a0
    b2 = b1
    b3 = b0
    return [b0, b1, b2, b3], [1.0, a1, a2, a3]


def get_bessel4(fc, sr=44100):
    wc = 2*np.pi*fc/_bessel4_3db
    k = 1/np.tan(wc/(2*sr))
    k2 = k*k
    a0 = (((k + 10)*k + 45)*k + 105)*k + 105
    a1 = (((-4*k - 20)*k2 + 210)*k + 420)/a0
    a2 = ((6*k2 - 90)*k2 + 630)/a0
    a3 = (((-4*k + 20)*k2 - 210)*k + 420)/a0
    a4 = ((((k - 10)*k + 45)*k - 105)*k + 105)/a0
    b0 = 105/a0
    b1 = 420/a0
    b2 = 630/a0
    b3 = b1
    b4 = b0
    return [b0, b1, b2, b3, b4], [1.0, a1, a2, a3, a4]

def get_hishelf(fc, db, S=1.0, sr=44100):
    A = 10.0**(db/40.0)
    wc = 2.0*np.pi*fc/float(sr)
    cosw = np.cos(wc)
    sinw = np.sin(wc)
    a = 0.5*sinw*np.sqrt((A + 1.0/A)*(1.0/S - 1.0) + 2.0)

    sqrtAa2 = 2.0*np.sqrt(A)*a
    Ap1 = A + 1.0
    Am1 = A - 1.0
    Ap1cosw = Ap1*cosw
    Am1cosw = Am1*cosw

    a0 = Ap1 - Am1cosw + sqrtAa2
    a1 = 2.0*(Am1 - Ap1cosw)/a0
    a2 = (Ap1 - Am1cosw - sqrtAa2)/a0
    b0 = A*(Ap1 + Am1cosw + sqrtAa2)/a0
    b1 = -2.0*A*(Am1 + Ap1cosw)/a0
    b2 = A*(Ap1 + Am1cosw - sqrtAa2)/a0

    return [b0, b1, b2], [1.0, a1, a2]

def get_series(b0, a0, b1, a1):
    b0r, a0r, b1r, a1r = list(b0), list(a0), list(b1), list(a1) # copy to reverse without changing filter arrays passed in
    b0r.reverse(); a0r.reverse(); b1r.reverse(); a1r.reverse() # reverse coefficient order from filter to polynomial
    b, a = list((poly(b0r)*poly(b1r)).coef), list((poly(a0r)*poly(a1r)).coef) # multiply polynomials
    b.reverse(); a.reverse() # reverse coefficient order from polynomial to filter
    return b, a

def get_airfilt200ms(sr=44100):
    blp, alp = get_bessel2(4500, sr)
    bhs, ahs = get_hishelf(275, -1.0/5.0, 0.5, sr)
    return blp, alp, bhs, ahs

def gen_cpp():
    blp, alp, bhs, ahs = get_airfilt200ms(44100)
    filename = 'AirFilter.h'
    text = ''
    text += '// Copyright Dan Price 2026.\n\n'
    text += '/////////////////////////////////\n'
    text += '//// DO NOT EDIT MANUALLY!!! ////\n'
    text += '//// GENERATED BY airfilt.py ////\n'
    text += '/////////////////////////////////\n\n'
    text += '#pragma once\n\n'
    text += '#include <complex>\n'
    text += '#include <cmath>\n\n'
    text += 'namespace AlbumBot::airfilt\n'
    text += '{\n'
    text += '\t// Biquad lopass filter section for 200ms of sound propagation\n'
    text += '\tnamespace lp200ms\n'
    text += '\t{\n'
    text += '\t\tconstexpr const double b0 = ' + str(blp[0]) + ';\n'
    text += '\t\tconstexpr const double b1 = ' + str(blp[1]) + ';\n'
    text += '\t\tconstexpr const double b2 = ' + str(blp[2]) + ';\n'
    text += '\t\tconstexpr const double a1 = ' + str(alp[1]) + ';\n'
    text += '\t\tconstexpr const double a2 = ' + str(alp[2]) + ';\n'
    text += '\t}\n\n'
    text += '\t// Biquad hishelf filter section for 200ms of sound propagation\n'
    text += '\tnamespace hs200ms\n'
    text += '\t{\n'
    text += '\t\tconstexpr const double b0 = ' + str(bhs[0]) + ';\n'
    text += '\t\tconstexpr const double b1 = ' + str(bhs[1]) + ';\n'
    text += '\t\tconstexpr const double b2 = ' + str(bhs[2]) + ';\n'
    text += '\t\tconstexpr const double a1 = ' + str(ahs[1]) + ';\n'
    text += '\t\tconstexpr const double a2 = ' + str(ahs[2]) + ';\n'
    text += '\t}\n\n'

    text += '\ttemplate<size_t nfreqs>\n'
    text += '\tinline double flerp(\n'
    text += '\t\tconst double x,\n'
    text += '\t\tconst double (&freqs)[nfreqs],\n'
    text += '\t\tconst double (&vals)[nfreqs],\n'
    text += '\t\tconst double (&slopes)[nfreqs])\n'
    text += '\t{\n'
    text += '\t\tsize_t idx = 0;\n'
    text += '\t\tfor (; idx < nfreqs; ++idx)\n'
    text += '\t\t\tif (x < freqs[idx])\n'
    text += '\t\t\t\tbreak;\n'
    text += '\t\tif (idx == 0)\n'
    text += '\t\t\treturn vals[0];\n'
    text += '\t\tif (idx == nfreqs)\n'
    text += '\t\t\t--idx; // Extend lerp from final two points\n'
    #text += '\t\tconst size_t loidx = hiidx - 1;\n'
    #text += '\t\tconst double x0 = freqs[loidx];\n'
    #text += '\t\tconst double x1 = freqs[hiidx];\n'
    #text += '\t\tconst double y0 = vals[loidx];\n'
    #text += '\t\tconst double y1 = vals[hiidx];\n'
    #text += '\t\treturn (y1 - y0)/(x1 - x0)*(x - x0) + y0;\n'
    text += '\t\treturn slopes[idx]*(x - freqs[idx]) + vals[idx];\n'

    #hiidx = 0
    #for freq in f:
        #if x < freq:
            #break
        #else:
            #hiidx += 1
    #if hiidx == 0:
        #return d[f[0]]
    #if hiidx == len(f):
        #hiidx -= 1 # extend lerp from final two points
    #loidx = hiidx - 1
    #x0 = f[loidx]
    #x1 = f[hiidx]
    #y0 = d[x0]
    #y1 = d[x1]
    #return (y1 - y0)/(x1 - x0)*(x - x0) + y0

    text += '\t}\n\n'

    wftext = '\tconstexpr const double warped_freqs[] = {\n'
    uftext = '\tconstexpr const double unwarp_factors[] = {\n'
    ustext = '\tconstexpr const double unwarp_slopes[] = {\n'
    ftext = '\tconstexpr const double freqs[] = {\n'
    ztext = '\tconstexpr const double zeroes[] = {\n'
    zstext = '\tconstexpr const double zeroes_slopes[] = {\n'
    for n, freq in enumerate(flerp.warped_freqs):
        wftext += '\t\t' + str(freq)
        uftext += '\t\t' + str(flerp.funwarp_dict[freq])
        ftext += '\t\t' + str(flerp.freqs[n])
        ztext += '\t\t' + str(flerp.f2z_dict[flerp.freqs[n]])
        if n == 0:
            ustext += '\t\t' + '0.0'
            zstext += '\t\t' + '0.0'
        else:
            x0 = flerp.warped_freqs[n - 1]
            x1 = freq
            y0 = flerp.funwarp_dict[x0]
            y1 = flerp.funwarp_dict[x1]
            ustext += '\t\t' + str((y1 - y0)/(x1 - x0))
            x0 = flerp.freqs[n - 1]
            x1 = flerp.freqs[n]
            y0 = flerp.f2z_dict[x0]
            y1 = flerp.f2z_dict[x1]
            zstext += '\t\t' + str((y1 - y0)/(x1 - x0))
        if n < len(flerp.warped_freqs) - 1:
            wftext += ','
            uftext += ','
            ustext += ','
            ftext += ','
            ztext += ','
            zstext += ','
        wftext += '\n'
        uftext += '\n'
        ustext += '\n'
        ftext += '\n'
        ztext += '\n'
        zstext += '\n'
    wftext += '\t};\n\n'
    uftext += '\t};\n\n'
    ustext += '\t};\n\n'
    ftext += '\t};\n\n'
    ztext += '\t};\n\n'
    zstext += '\t};\n\n'

    text += wftext
    text += uftext
    text += ustext
    text += ftext
    text += ztext
    text += zstext

    text += '\tinline double fprewarp(const double fc44100)\n'
    text += '\t{\n'
    text += '\t\treturn flerp(fc44100, warped_freqs, unwarp_factors, unwarp_slopes);\n'
    text += '\t}\n\n'

    text += '\tinline double f2z(const double fc_prewarped)\n'
    text += '\t{\n'
    text += '\t\treturn flerp(fc_prewarped, freqs, zeroes, zeroes_slopes);\n'
    text += '\t}\n\n'

    text += '\tstruct biquad\n'
    text += '\t{\n'
    text += '\t\tdouble b0;\n'
    text += '\t\tdouble b1;\n'
    text += '\t\tdouble b2;\n'
    text += '\t\tdouble a1;\n'
    text += '\t\tdouble a2;\n'
    text += '\t};\n\n'

    text += '\tinline biquad get_bessel2_lpf(const double fc, const unsigned long sr = 44100)\n'
    text += '\t{\n'
    text += '\t\tstatic constexpr const double tau = ' + str(2.0*np.pi) + ';\n'
    text += '\t\tstatic constexpr const double dt44100 = 1.0/44100.0;\n'
    text += '\t\tstatic constexpr const double bessel2_3db_factor = ' + str(_bessel2_3db) + ';\n'
    text += '\t\tstatic constexpr const double w0_factor = tau*dt44100/bessel2_3db_factor;\n\n'

    text += '\t\tbiquad filt;\n\n'

    text += '\t\tconst double fc44100 = (sr == 44100) ? fc : fc*44100.0/double(sr);\n'
    text += '\t\tconst double fc_prewarped = fprewarp(fc44100)*fc44100;\n'
    text += '\t\tconst double z_inf = f2z(fc_prewarped);\n'

    #fc44100 = fc if sr == 44100 else fc*44100/sr
    #f0 = flerp.funwarp(fc44100)*fc44100
    #z0 = flerp.f2z(f0)
    #return get_bessel2_zinf(f0, sr=44100, z_inf=z0)

    text += '\t\tconst double w0 = w0_factor*fc_prewarped;\n'
    text += '\t\tconst std::complex<double> eiw0 = std::polar(1.0, w0);\n'
    text += '\t\tconst double _2cosw0 = 2.0*eiw0.real();\n'
    text += '\t\tconst double _3sinw0 = 3.0*eiw0.imag();\n'
    text += '\t\tconst double oa0 = 1.0/(4.0 - _2cosw0 + _3sinw0);\n'
    text += '\t\tfilt.a1 = (4.0 - 8.0*eiw0.real())*oa0;\n'
    text += '\t\tfilt.a2 = (4.0 - _2cosw0 - _3sinw0)*oa0;\n'
    text += '\t\tconst double _1mzinf = 1.0 - z_inf;\n'
    text += '\t\tfilt.b0 = 12.0*(1.0 - eiw0.real())/(_1mzinf*_1mzinf)*oa0;\n'
    text += '\t\tconst double zinfb0 = z_inf*filt.b0;\n'
    text += '\t\tfilt.b1 = -2.0*zinfb0;\n'
    text += '\t\tfilt.b2 = z_inf*zinfb0;\n'
    text += '\t\treturn filt;\n'
    text += '\t}\n\n'

    #w0 = 2.0*np.pi*fc/sr/_bessel2_3db
    #eiw0 = np.exp(complex(0.0, w0))
    #_2cosw0 = 2.0*eiw0.real
    #_3sinw0 = 3.0*eiw0.imag
    #oa0 = 1.0/(4.0 - _2cosw0 + _3sinw0)
    #a1 = (4.0 - 8.0*eiw0.real)*oa0
    #a2 = (4.0 - _2cosw0 - _3sinw0)*oa0
    #_1mzinf = 1.0 - z_inf
    #b0 = 12.0*(1.0 - eiw0.real)/(_1mzinf*_1mzinf)*oa0
    #zinfb0 = z_inf*b0
    #b1 = -2.0*zinfb0
    #b2 = z_inf*zinfb0
    #return [b0, b1, b2], [1.0, a1, a2]

    text += '\tinline double time2freq(const double time)\n'
    text += '\t{\n'
    text += '\t\tconst double log2t = std::log2(time);\n'
    text += '\t\treturn std::exp2(((' + str(t2f.log2cubic[0]) + '*log2t + '
    text += str(t2f.log2cubic[1]) + ')*log2t + ' + str(t2f.log2cubic[2]) + ')*log2t + ' + str(t2f.log2cubic[3]) + ');\n'
    text += '\t}\n\n'

    text += '\tinline biquad get_airfilt(const double time, const unsigned long sr = 44100)\n'
    text += '\t{\n'
    text += '\t\treturn get_bessel2_lpf(time2freq(time), sr);\n'
    text += '\t}\n'

    text += '}\n\n'
    with open(filename, 'w') as f:
        f.write(text)

def plot(arrb, arra, sr=44100):
    for n in range(min(len(arrb), len(arra))):
        w, h = sig.freqz(arrb[n], arra[n], fs=sr)
        plt.plot(w, 20*np.log10(abs(h)))

def show():
    plt.xlim(15, 25000)
    plt.ylim(-25, 1)
    plt.xscale('log')
    plt.show()

def test0():
    b2, a2 = get_bessel2(5000, 44100)
    b3, a3 = get_bessel3(5000, 44100)
    b4, a4 = get_bessel4(5000, 44100)
    b2sq, a2sq = get_series(b2, a2, b2, a2)
    b3sq, a3sq = get_series(b3, a3, b3, a3)
    b4sq, a4sq = get_series(b4, a4, b4, a4)
    plot([b2, b3, b4, b2sq, b3sq, b4sq], [a2, a3, a4, a2sq, a3sq, a4sq], 44100)
    show()

def test1():
    b2, a2 = get_bessel2(5000, 44100)
    arrb, arra = [b2], [a2]
    blast, alast = b2, a2
    for n in range(20):
        blast, alast = get_series(blast, alast, b2, a2)
        arrb.append(blast)
        arra.append(alast)
    plot(arrb, arra, 44100)
    show()

def test2():
    #blp, alp = get_bessel2(4500, 44100)
    #bhs, ahs = get_hishelf(300, -1.0/4.5, 0.4, 44100)
    #b, a = get_series(blp, alp, bhs, ahs)
    b, a = get_series(*get_airfilt200ms(44100))
    w, h = sig.freqz(b, a, fs=44100, worN=2048)
    h = 20*np.log10(abs(h))
    hlast = np.array(h)
    plt.plot(w, hlast)
    for n in range(39):
        hlast += h
        plt.plot(w, hlast)
    show()

def test():
    test2()

if __name__ == '__main__':
    test()

