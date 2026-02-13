import blep
import numpy as np

def main():

    blep_len = 60
    zc = 25
    #get_blep_res, blerps, coeffs, bl, sf, wsf = blep.blep_res(blep_len, zc, blepf=blep.blep3, wfunc=blep.hann, cubicsf=blep.blep_spline_septic2)
    get_blep_res, blerps, coeffs, bl, sf, wsf = blep.blep_res(blep_len, zc, blepf=blep.blep3, wfunc=blep.quart, cubicsf=blep.blep_spline_septic3)

    blep_len_fast = 20
    zc_fast = 7
    #get_blep_res_fast, blerps_fast, coeffs_fast, bl_fast, sf_fast, wsf_fast = blep.blep_res(blep_len_fast, zc_fast, blepf=blep.blep3, wfunc=blep.hann, cubicsf=blep.blep_spline_nonic)
    get_blep_res_fast, blerps_fast, coeffs_fast, bl_fast, sf_fast, wsf_fast = blep.blep_res(blep_len_fast, zc_fast, blepf=blep.blep3, wfunc=blep.quart, cubicsf=blep.blep_spline_nonic2)

    blep_len_xfast = 16
    zc_xfast = 5
    #get_blep_res_xfast, blerps_xfast, coeffs_xfast, bl_xfast, sf_xfast, wsf_xfast = blep.blep_res(blep_len_xfast, zc_xfast, blepf=blep.blep3, wfunc=blep.hann, cubicsf=blep.blep_spline_quintic2)
    get_blep_res_xfast, blerps_xfast, coeffs_xfast, bl_xfast, sf_xfast, wsf_xfast = blep.blep_res(blep_len_xfast, zc_xfast, blepf=blep.blep3, wfunc=blep.quart, cubicsf=blep.blep_spline_quintic4)

    mblep_len = 60
    mzc = 25
    mget_blep_res, mblerps, mcoeffs, mbl, msf, mwsf = blep.blep_res(blep_len, zc, blepf=blep.wmonostep, wfunc=blep.quart, cubicsf=blep.blep_spline_septic3)

    mblep_len_fast = 20
    mzc_fast = 7
    mget_blep_res_fast, mblerps_fast, mcoeffs_fast, mbl_fast, msf_fast, mwsf_fast = blep.blep_res(blep_len_fast, zc_fast, blepf=blep.wmonostep, wfunc=blep.quart, cubicsf=blep.blep_spline_nonic2)

    mblep_len_xfast = 16
    mzc_xfast = 5
    mget_blep_res_xfast, mblerps_xfast, mcoeffs_xfast, mbl_xfast, msf_xfast, mwsf_xfast = blep.blep_res(blep_len_xfast, zc_xfast, blepf=blep.wmonostep, wfunc=blep.quart, cubicsf=blep.blep_spline_quintic4)

    ripplestep = blep.wripplestep(blep.db_to_ripple(3))
    rblep_len = 60
    rzc = 25
    rget_blep_res, rblerps, rcoeffs, rbl, rsf, rwsf = blep.blep_res(blep_len, zc, blepf=ripplestep, wfunc=blep.quart, cubicsf=blep.blep_spline_septic3)

    rblep_len_fast = 20
    rzc_fast = 7
    rget_blep_res_fast, rblerps_fast, rcoeffs_fast, rbl_fast, rsf_fast, rwsf_fast = blep.blep_res(blep_len_fast, zc_fast, blepf=ripplestep, wfunc=blep.quart, cubicsf=blep.blep_spline_nonic2)

    rblep_len_xfast = 16
    rzc_xfast = 5
    rget_blep_res_xfast, rblerps_xfast, rcoeffs_xfast, rbl_xfast, rsf_xfast, rwsf_xfast = blep.blep_res(blep_len_xfast, zc_xfast, blepf=ripplestep, wfunc=blep.quart, cubicsf=blep.blep_spline_quintic4)

    halfripplestep = blep.wripplestep(0.5)
    hblep_len = 60
    hzc = 25
    hget_blep_res, hblerps, hcoeffs, hbl, hsf, hwsf = blep.blep_res(blep_len, zc, blepf=halfripplestep, wfunc=blep.quart, cubicsf=blep.blep_spline_septic3)

    hblep_len_fast = 20
    hzc_fast = 7
    hget_blep_res_fast, hblerps_fast, hcoeffs_fast, hbl_fast, hsf_fast, hwsf_fast = blep.blep_res(blep_len_fast, zc_fast, blepf=halfripplestep, wfunc=blep.quart, cubicsf=blep.blep_spline_nonic2)

    hblep_len_xfast = 16
    hzc_xfast = 5
    hget_blep_res_xfast, hblerps_xfast, hcoeffs_xfast, hbl_xfast, hsf_xfast, hwsf_xfast = blep.blep_res(blep_len_xfast, zc_xfast, blepf=halfripplestep, wfunc=blep.quart, cubicsf=blep.blep_spline_quintic4)

    write_to_infinisaw_files(
        coeffs, blep_len, zc,
        coeffs_fast, blep_len_fast, zc_fast,
        coeffs_xfast, blep_len_xfast, zc_xfast,
        mcoeffs, mblep_len, mzc,
        mcoeffs_fast, mblep_len_fast, mzc_fast,
        mcoeffs_xfast, mblep_len_xfast, mzc_xfast,
        rcoeffs, rblep_len, rzc,
        rcoeffs_fast, rblep_len_fast, rzc_fast,
        rcoeffs_xfast, rblep_len_xfast, rzc_xfast,
        hcoeffs, hblep_len, hzc,
        hcoeffs_fast, hblep_len_fast, hzc_fast,
        hcoeffs_xfast, hblep_len_xfast, hzc_xfast)

def write_to_infinisaw_files(
        coeffs, blep_len, zc,
        coeffs_fast, blep_len_fast, zc_fast,
        coeffs_xfast, blep_len_xfast, zc_xfast,
        mcoeffs, mblep_len, mzc,
        mcoeffs_fast, mblep_len_fast, mzc_fast,
        mcoeffs_xfast, mblep_len_xfast, mzc_xfast,
        rcoeffs, rblep_len, rzc,
        rcoeffs_fast, rblep_len_fast, rzc_fast,
        rcoeffs_xfast, rblep_len_xfast, rzc_xfast,
        hcoeffs, hblep_len, hzc,
        hcoeffs_fast, hblep_len_fast, hzc_fast,
        hcoeffs_xfast, hblep_len_xfast, hzc_xfast):
    with open("InfiniSaw.gen.h", 'w') as f:
        f.write('// Copyright Dan Price. All rights reserved.\n\n')
        f.write('#include "Quintic.h"\n')
        f.write('#include "Septic.h"\n')
        f.write('#include "Nonic.h"\n\n')
        f.write('namespace AlbumBot\n')
        f.write('{\n')

        f.write('\tconstexpr const size_t BLEP_LENGTH = ' + str(blep_len) + ';\n')
        f.write('\tconstexpr const size_t BLEP_ZERO_CROSSINGS = ' + str(zc) + ';\n')
        f.write('\tconstexpr const size_t BLEP_POLYS = BLEP_LENGTH - 1;\n')
        f.write('\tconstexpr const size_t BLEP_PEEK = BLEP_POLYS / 2;\n')
        f.write('\tusing BlepPolyType = Septic64;\n')
        f.write('\tconstexpr const size_t BLEP_LENGTH_FAST = ' + str(blep_len_fast) + ';\n')
        f.write('\tconstexpr const size_t BLEP_ZERO_CROSSINGS_FAST = ' + str(zc_fast) + ';\n')
        f.write('\tconstexpr const size_t BLEP_POLYS_FAST = BLEP_LENGTH_FAST - 1;\n')
        f.write('\tconstexpr const size_t BLEP_PEEK_FAST = BLEP_POLYS_FAST / 2;\n')
        f.write('\tusing BlepPolyType_Fast = Nonic64;\n')
        f.write('\tconstexpr const size_t BLEP_LENGTH_XFAST = ' + str(blep_len_xfast) + ';\n')
        f.write('\tconstexpr const size_t BLEP_ZERO_CROSSINGS_XFAST = ' + str(zc_xfast) + ';\n')
        f.write('\tconstexpr const size_t BLEP_POLYS_XFAST = BLEP_LENGTH_XFAST - 1;\n')
        f.write('\tconstexpr const size_t BLEP_PEEK_XFAST = BLEP_POLYS_XFAST / 2;\n')
        f.write('\tusing BlepPolyType_Xfast = Quintic64;\n')

        f.write('\tconstexpr const size_t MBLEP_LENGTH = ' + str(mblep_len) + ';\n')
        f.write('\tconstexpr const size_t MBLEP_ZERO_CROSSINGS = ' + str(mzc) + ';\n')
        f.write('\tconstexpr const size_t MBLEP_POLYS = MBLEP_LENGTH - 1;\n')
        f.write('\tconstexpr const size_t MBLEP_PEEK = MBLEP_POLYS / 2;\n')
        f.write('\tusing MBlepPolyType = Septic64;\n')
        f.write('\tconstexpr const size_t MBLEP_LENGTH_FAST = ' + str(mblep_len_fast) + ';\n')
        f.write('\tconstexpr const size_t MBLEP_ZERO_CROSSINGS_FAST = ' + str(mzc_fast) + ';\n')
        f.write('\tconstexpr const size_t MBLEP_POLYS_FAST = MBLEP_LENGTH_FAST - 1;\n')
        f.write('\tconstexpr const size_t MBLEP_PEEK_FAST = MBLEP_POLYS_FAST / 2;\n')
        f.write('\tusing MBlepPolyType_Fast = Nonic64;\n')
        f.write('\tconstexpr const size_t MBLEP_LENGTH_XFAST = ' + str(mblep_len_xfast) + ';\n')
        f.write('\tconstexpr const size_t MBLEP_ZERO_CROSSINGS_XFAST = ' + str(mzc_xfast) + ';\n')
        f.write('\tconstexpr const size_t MBLEP_POLYS_XFAST = MBLEP_LENGTH_XFAST - 1;\n')
        f.write('\tconstexpr const size_t MBLEP_PEEK_XFAST = MBLEP_POLYS_XFAST / 2;\n')
        f.write('\tusing MBlepPolyType_Xfast = Quintic64;\n')

        f.write('\tconstexpr const size_t RBLEP_LENGTH = ' + str(rblep_len) + ';\n')
        f.write('\tconstexpr const size_t RBLEP_ZERO_CROSSINGS = ' + str(rzc) + ';\n')
        f.write('\tconstexpr const size_t RBLEP_POLYS = RBLEP_LENGTH - 1;\n')
        f.write('\tconstexpr const size_t RBLEP_PEEK = RBLEP_POLYS / 2;\n')
        f.write('\tusing RBlepPolyType = Septic64;\n')
        f.write('\tconstexpr const size_t RBLEP_LENGTH_FAST = ' + str(rblep_len_fast) + ';\n')
        f.write('\tconstexpr const size_t RBLEP_ZERO_CROSSINGS_FAST = ' + str(rzc_fast) + ';\n')
        f.write('\tconstexpr const size_t RBLEP_POLYS_FAST = RBLEP_LENGTH_FAST - 1;\n')
        f.write('\tconstexpr const size_t RBLEP_PEEK_FAST = RBLEP_POLYS_FAST / 2;\n')
        f.write('\tusing RBlepPolyType_Fast = Nonic64;\n')
        f.write('\tconstexpr const size_t RBLEP_LENGTH_XFAST = ' + str(rblep_len_xfast) + ';\n')
        f.write('\tconstexpr const size_t RBLEP_ZERO_CROSSINGS_XFAST = ' + str(rzc_xfast) + ';\n')
        f.write('\tconstexpr const size_t RBLEP_POLYS_XFAST = RBLEP_LENGTH_XFAST - 1;\n')
        f.write('\tconstexpr const size_t RBLEP_PEEK_XFAST = RBLEP_POLYS_XFAST / 2;\n')
        f.write('\tusing RBlepPolyType_Xfast = Quintic64;\n')

        f.write('\tconstexpr const size_t HBLEP_LENGTH = ' + str(hblep_len) + ';\n')
        f.write('\tconstexpr const size_t HBLEP_ZERO_CROSSINGS = ' + str(hzc) + ';\n')
        f.write('\tconstexpr const size_t HBLEP_POLYS = HBLEP_LENGTH - 1;\n')
        f.write('\tconstexpr const size_t HBLEP_PEEK = HBLEP_POLYS / 2;\n')
        f.write('\tusing HBlepPolyType = Septic64;\n')
        f.write('\tconstexpr const size_t HBLEP_LENGTH_FAST = ' + str(hblep_len_fast) + ';\n')
        f.write('\tconstexpr const size_t HBLEP_ZERO_CROSSINGS_FAST = ' + str(hzc_fast) + ';\n')
        f.write('\tconstexpr const size_t HBLEP_POLYS_FAST = HBLEP_LENGTH_FAST - 1;\n')
        f.write('\tconstexpr const size_t HBLEP_PEEK_FAST = HBLEP_POLYS_FAST / 2;\n')
        f.write('\tusing HBlepPolyType_Fast = Nonic64;\n')
        f.write('\tconstexpr const size_t HBLEP_LENGTH_XFAST = ' + str(hblep_len_xfast) + ';\n')
        f.write('\tconstexpr const size_t HBLEP_ZERO_CROSSINGS_XFAST = ' + str(hzc_xfast) + ';\n')
        f.write('\tconstexpr const size_t HBLEP_POLYS_XFAST = HBLEP_LENGTH_XFAST - 1;\n')
        f.write('\tconstexpr const size_t HBLEP_PEEK_XFAST = HBLEP_POLYS_XFAST / 2;\n')
        f.write('\tusing HBlepPolyType_Xfast = Quintic64;\n')

        f.write('}\n\n')

    with open("InfiniSaw.cpp", 'w') as f:
        f.write('#include "InfiniSaw.h"\n\n')
        f.write('namespace AlbumBot\n')
        f.write('{\n')

        f.write('\tstd::array<BlepPolyType, BLEP_POLYS> InfiniSaw::blep_precise = {\n')
        comma = ''
        for poly in coeffs:
            f.write('\t\t')
            f.write(comma)
            comma = ', '
            f.write('BlepPolyType(')
            subcomma = ''
            for c in poly:
                f.write(subcomma + str(np.float64(c)))
                subcomma = ', '
            f.write(')\n')
        f.write('\t};\n\n')

        f.write('\tstd::array<BlepPolyType_Fast, BLEP_POLYS_FAST> InfiniSaw::blep_fast = {\n')
        comma = ''
        for poly in coeffs_fast:
            f.write('\t\t')
            f.write(comma)
            comma = ', '
            f.write('BlepPolyType_Fast(')
            subcomma = ''
            for c in poly:
                f.write(subcomma + str(np.float64(c)))
                subcomma = ', '
            f.write(')\n')
        f.write('\t};\n\n')

        f.write('\tstd::array<BlepPolyType_Xfast, BLEP_POLYS_XFAST> InfiniSaw::blep_xfast = {\n')
        comma = ''
        for poly in coeffs_xfast:
            f.write('\t\t')
            f.write(comma)
            comma = ', '
            f.write('BlepPolyType_Xfast(')
            subcomma = ''
            for c in poly:
                f.write(subcomma + str(np.float64(c)))
                subcomma = ', '
            f.write(')\n')
        f.write('\t};\n\n')

        f.write('\tstd::array<MBlepPolyType, MBLEP_POLYS> InfiniSaw::mblep_precise = {\n')
        comma = ''
        for poly in mcoeffs:
            f.write('\t\t')
            f.write(comma)
            comma = ', '
            f.write('MBlepPolyType(')
            subcomma = ''
            for c in poly:
                f.write(subcomma + str(np.float64(c)))
                subcomma = ', '
            f.write(')\n')
        f.write('\t};\n\n')

        f.write('\tstd::array<MBlepPolyType_Fast, MBLEP_POLYS_FAST> InfiniSaw::mblep_fast = {\n')
        comma = ''
        for poly in mcoeffs_fast:
            f.write('\t\t')
            f.write(comma)
            comma = ', '
            f.write('MBlepPolyType_Fast(')
            subcomma = ''
            for c in poly:
                f.write(subcomma + str(np.float64(c)))
                subcomma = ', '
            f.write(')\n')
        f.write('\t};\n\n')

        f.write('\tstd::array<MBlepPolyType_Xfast, MBLEP_POLYS_XFAST> InfiniSaw::mblep_xfast = {\n')
        comma = ''
        for poly in mcoeffs_xfast:
            f.write('\t\t')
            f.write(comma)
            comma = ', '
            f.write('MBlepPolyType_Xfast(')
            subcomma = ''
            for c in poly:
                f.write(subcomma + str(np.float64(c)))
                subcomma = ', '
            f.write(')\n')
        f.write('\t};\n\n')

        f.write('\tstd::array<RBlepPolyType, RBLEP_POLYS> InfiniSaw::rblep_precise = {\n')
        comma = ''
        for poly in rcoeffs:
            f.write('\t\t')
            f.write(comma)
            comma = ', '
            f.write('RBlepPolyType(')
            subcomma = ''
            for c in poly:
                f.write(subcomma + str(np.float64(c)))
                subcomma = ', '
            f.write(')\n')
        f.write('\t};\n\n')

        f.write('\tstd::array<RBlepPolyType_Fast, RBLEP_POLYS_FAST> InfiniSaw::rblep_fast = {\n')
        comma = ''
        for poly in rcoeffs_fast:
            f.write('\t\t')
            f.write(comma)
            comma = ', '
            f.write('RBlepPolyType_Fast(')
            subcomma = ''
            for c in poly:
                f.write(subcomma + str(np.float64(c)))
                subcomma = ', '
            f.write(')\n')
        f.write('\t};\n\n')

        f.write('\tstd::array<RBlepPolyType_Xfast, RBLEP_POLYS_XFAST> InfiniSaw::rblep_xfast = {\n')
        comma = ''
        for poly in rcoeffs_xfast:
            f.write('\t\t')
            f.write(comma)
            comma = ', '
            f.write('RBlepPolyType_Xfast(')
            subcomma = ''
            for c in poly:
                f.write(subcomma + str(np.float64(c)))
                subcomma = ', '
            f.write(')\n')
        f.write('\t};\n\n')

        f.write('\tstd::array<HBlepPolyType, HBLEP_POLYS> InfiniSaw::hblep_precise = {\n')
        comma = ''
        for poly in hcoeffs:
            f.write('\t\t')
            f.write(comma)
            comma = ', '
            f.write('HBlepPolyType(')
            subcomma = ''
            for c in poly:
                f.write(subcomma + str(np.float64(c)))
                subcomma = ', '
            f.write(')\n')
        f.write('\t};\n\n')

        f.write('\tstd::array<HBlepPolyType_Fast, HBLEP_POLYS_FAST> InfiniSaw::hblep_fast = {\n')
        comma = ''
        for poly in hcoeffs_fast:
            f.write('\t\t')
            f.write(comma)
            comma = ', '
            f.write('HBlepPolyType_Fast(')
            subcomma = ''
            for c in poly:
                f.write(subcomma + str(np.float64(c)))
                subcomma = ', '
            f.write(')\n')
        f.write('\t};\n\n')

        f.write('\tstd::array<HBlepPolyType_Xfast, HBLEP_POLYS_XFAST> InfiniSaw::hblep_xfast = {\n')
        comma = ''
        for poly in hcoeffs_xfast:
            f.write('\t\t')
            f.write(comma)
            comma = ', '
            f.write('HBlepPolyType_Xfast(')
            subcomma = ''
            for c in poly:
                f.write(subcomma + str(np.float64(c)))
                subcomma = ', '
            f.write(')\n')
        f.write('\t};\n')

        f.write('}\n\n')

def draw_blep(coeffs):
    pass

if __name__ == '__main__':
    main()

