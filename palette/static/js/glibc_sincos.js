/*
 * glibc 2.36 sin and cos for doubles, ported to JavaScript.
 *
 * IBM Accurate Mathematical Library
 * written by International Business Machines Corp.
 * Copyright (C) 2001-2022 Free Software Foundation, Inc.
 * JavaScript port: Copyright 2026 Insolent Construct.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation; either version 2.1 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program; if not, see <https://www.gnu.org/licenses/>.
 */

// From sysdeps/ieee754/dbl-64/s_sin.c, usncs.h and sincostab.c, the generic
// (non-FMA) path that x86-64 glibc runs without AVX2/FMA. The release WAV is
// encoded by numpy on that libm, so the page needs the same last bits: a
// single one decides between output samples 32766 and 32767. JavaScript
// doubles round each operation, as the non-FMA C build does.

const TABLE_BITS = [
  0x0000000000000000n, 0x0000000000000000n, 0x3ff0000000000000n, 0x0000000000000000n,
  0x3f7fffeaaaaeeeefn, 0xbc1e45e2ec67b77cn, 0x3fefffc000155552n, 0x3c8f4a01a0196daen,
  0x3f8fffaaaaeeeed5n, 0xbc02ab639a9f0777n, 0x3fefff000155549fn, 0x3c828a28a03a5ef3n,
  0x3f97ff7001033255n, 0x3bfefe2b51527336n, 0x3feffdc006bff7e6n, 0x3c8ae6dae86977bdn,
  0x3f9ffeaaaeeee86fn, 0xbc3cd406fb224ae2n, 0x3feffc00155527d3n, 0xbc83b54492d89b5bn,
  0x3fa3feb2b12d45d5n, 0x3c34ec54203d1c11n, 0x3feff9c03414a7ban, 0x3c6991f4be6c59bfn,
  0x3fa7fdc01032fba9n, 0xbc4599bdf46e997an, 0x3feff7006bfdf99fn, 0xbc78b3b560648d5fn,
  0x3fabfc6d78586dacn, 0x3c18e4fd03dbf236n, 0x3feff3c0c8103a31n, 0x3c74856dbddc0e66n,
  0x3faffaaaeeed4edbn, 0xbc42d16d32684b69n, 0x3feff0015549f4d3n, 0x3c8328387b99426fn,
  0x3fb1fc343d808befn, 0xbc5f3d32e6f3be4fn, 0x3fefebc222a8ef9fn, 0x3c57934934f54c77n,
  0x3fb3facb12d1755bn, 0xbc5921915299468cn, 0x3fefe7034129ef6fn, 0xbc6cbf4337c96f97n,
  0x3fb5f911fd10b737n, 0xbc50184f02be9102n, 0x3fefe1c4c3c873ebn, 0xbc35a9c9057c4a02n,
  0x3fb7f701032550e4n, 0x3c3afc2d1800501an, 0x3fefdc06bf7e6b9bn, 0x3c831902b535f8dbn,
  0x3fb9f4902d55d1f9n, 0x3c52696d7eac1dc1n, 0x3fefd5c94b43e000n, 0xbc62e768cb4f92f9n,
  0x3fbbf1b78568391dn, 0x3c5e91841dea4cc8n, 0x3fefcf0c800e99b1n, 0x3c6ea3d786d186acn,
  0x3fbdee6f16c1cce6n, 0xbc450f8e2fb71673n, 0x3fefc7d078d1bc88n, 0x3c8075d2447db685n,
  0x3fbfeaaeee86ee36n, 0xbc4afcb2bcc6f03bn, 0x3fefc015527d5bd3n, 0x3c8b68f35094efb8n,
  0x3fc0f3378ddd71d1n, 0x3c6d8468724f0f9en, 0x3fefb7db2bfe0695n, 0x3c821dadf4f65ab1n,
  0x3fc1f0d3d7afceafn, 0xbc66ef95099769a5n, 0x3fefaf22263c4bd3n, 0xbc552ace133a2769n,
  0x3fc2ee285e4ab88fn, 0xbc6e4d0f05dee058n, 0x3fefa5ea641c36f2n, 0x3c404da6ed17cc7cn,
  0x3fc3eb312c5d66cbn, 0x3c647d666b66cb91n, 0x3fef9c340a7cc428n, 0x3c8c5b6b063b7462n,
  0x3fc4e7ea4dc5f27bn, 0x3c5949db2ac072fcn, 0x3fef91ff40374d01n, 0xbc67d03f4d3a9e4cn,
  0x3fc5e44fcfa126f3n, 0xbc66f443063f89b6n, 0x3fef874c2e1eecf6n, 0xbc8c6514e1332b16n,
  0x3fc6e05dc05a4d4cn, 0xbbd32c5c8b81c940n, 0x3fef7c1afeffde24n, 0xbc78f55bc47540b1n,
  0x3fc7dc102fbaf2b5n, 0x3c45ab50e23c97c3n, 0x3fef706bdf9ece1cn, 0xbc8698c80c36dcb4n,
  0x3fc8d7632efaa944n, 0xbc620fa262cbb953n, 0x3fef643efeb82acdn, 0x3c76b00ac1fe28acn,
  0x3fc9d252d0cec312n, 0x3c59c43d80b1137dn, 0x3fef57948cff6797n, 0x3c6e3a0d3e03b1d5n,
  0x3fcaccdb297a0765n, 0xbc59883b57d6cdebn, 0x3fef4a6cbd1e3a79n, 0x3c813df0edaebb57n,
  0x3fcbc6f84edc6199n, 0x3c69c1a56a7b0cabn, 0x3fef3cc7c3b3d16en, 0xbc621a3ad28a3494n,
  0x3fccc0a6588289a3n, 0xbc6868d09bc87c6bn, 0x3fef2ea5d753ffedn, 0x3c8cc4215f56d583n,
  0x3fcdb9e15fb5a5d0n, 0xbc632e20d6cc6fc2n, 0x3fef20073086649fn, 0x3c7b940416c1984bn,
  0x3fceb2a57f8ae5a3n, 0xbc60be06af572cebn, 0x3fef10ec09c5873bn, 0x3c8d9072762c1283n,
  0x3fcfaaeed4f31577n, 0xbc615d88508e32b8n, 0x3fef01549f7deea1n, 0x3c8d3c1e99e5cafdn,
  0x3fd0515cbf65155cn, 0xbc79b8c29dfd8ec8n, 0x3feef141300d2f26n, 0xbc82aa1b08ded372n,
  0x3fd0cd00cef36436n, 0xbc79fb0a0c93e2b5n, 0x3feee0b1fbc0f11cn, 0xbc4bfd2380bbc3b1n,
  0x3fd14861aa94ddebn, 0xbc6be881b5b615a4n, 0x3feecfa744d5efa1n, 0xbc556d0a4af541d0n,
  0x3fd1c37d64c6b876n, 0x3c746076fe0dcff5n, 0x3feebe214f76efa8n, 0xbc802f9f12ba543en,
  0x3fd23e52111aaf36n, 0xbc74f080334eff18n, 0x3feeac2061bbaf4fn, 0x3c62c1d53e94658dn,
  0x3fd2b8ddc43eb49fn, 0x3c61553899f2d807n, 0x3fee99a4c3a7cd83n, 0xbc82264b1bc53ce8n,
  0x3fd3331e94049f87n, 0x3c7e0cb6b40c302cn, 0x3fee86aebf29a9edn, 0x3c89397afdbb58a7n,
  0x3fd3ad129769d3d8n, 0x3c003d5504878398n, 0x3fee733ea0193d40n, 0xbc86428b3546ce13n,
  0x3fd426b7e69ee697n, 0xbc7f09c75705c59fn, 0x3fee5f54b436e9d0n, 0x3c87eb0fd02fc8bcn,
  0x3fd4a00c9b0f3d20n, 0x3c7823ba6bb08eadn, 0x3fee4af14b2a449cn, 0xbc868ca02e8a6833n,
  0x3fd5190ecf68a77an, 0x3c7b357155eef0f3n, 0x3fee3614b680d6a5n, 0xbc727793aa015237n,
  0x3fd591bc9fa2f597n, 0x3c67c74bac3fe0cbn, 0x3fee20bf49acd6c1n, 0xbc5660aec7ef636cn,
  0x3fd60a1429078775n, 0x3c5b1fd80ba89133n, 0x3fee0af15a03dbcen, 0x3c5fe8e702771ae6n,
  0x3fd682138a38d7f7n, 0xbc7d889202444aadn, 0x3fedf4ab3ebd875en, 0xbc8e2d8a7e6736c4n,
  0x3fd6f9b8e33a0255n, 0x3c742bc14ee9da0dn, 0x3feddded50f228d6n, 0xbc6e80c8d42ba2bfn,
  0x3fd7710255764214n, 0xbc66ead7314bb6cen, 0x3fedc6b7eb995912n, 0x3c54b364776dcd35n,
  0x3fd7e7ee03c86d4en, 0xbc7b63bcdabf5af2n, 0x3fedaf0b6b888e83n, 0x3c8a249e2b5e5cean,
  0x3fd85e7a12826949n, 0x3c78a40e9b5face0n, 0x3fed96e82f71a9dcn, 0x3c8ff61bd5d2039dn,
  0x3fd8d4a4a774992fn, 0x3c744a02ea766326n, 0x3fed7e4e97e17b4an, 0xbc63b770352bed94n,
  0x3fd94a6be9f546c5n, 0xbc769ce13e683f58n, 0x3fed653f073e4040n, 0xbc876236434bec37n,
  0x3fd9bfce02e80510n, 0x3c709e39a320b0a4n, 0x3fed4bb9e1c619e0n, 0x3c8f34bb77858f61n,
  0x3fda34c91cc50ccan, 0xbc5a310e3b50cecdn, 0x3fed31bf8d8d7c06n, 0x3c7e60dd3089cbddn,
  0x3fdaa95b63a09277n, 0xbc66293eb13c0381n, 0x3fed1750727d94f0n, 0x3c80d52b1ec1a48en,
  0x3fdb1d8305321617n, 0xbc7ae242cb99f519n, 0x3fecfc6cfa52ad9fn, 0x3c88b5b5508f2a0dn,
  0x3fdb913e30dbac43n, 0xbc7e38ad2f6c3ff1n, 0x3fece115909a82e5n, 0x3c81f139bb31109an,
  0x3fdc048b17b140a3n, 0x3c619fe6757e9fa7n, 0x3fecc54aa2b2972en, 0x3c64ee162ba83a98n,
  0x3fdc7767ec7fd19en, 0xbc5eb14d1a3d5826n, 0x3feca90c9fc67d0bn, 0xbc646a81485e3462n,
  0x3fdce9d2e3d4a51fn, 0xbc62fc8a12dae298n, 0x3fec8c5bf8ce1a84n, 0x3c7ab3d1a1590123n,
  0x3fdd5bca34047661n, 0x3c728a44a75fc29cn, 0x3fec6f39208be53bn, 0xbc8741dbfbaadb42n,
  0x3fddcd4c15329c9an, 0x3c70d4c6e171fd9an, 0x3fec51a48b8b175en, 0xbc61bbb43b9aa880n,
  0x3fde3e56c1582a69n, 0xbc50a4821099f88fn, 0x3fec339eb01ddd81n, 0xbc8caaf5ee82c5c0n,
  0x3fdeaee8744b05f0n, 0xbc5789b43c9b027dn, 0x3fec1528065b7d50n, 0xbc8892111312e828n,
  0x3fdf1eff6bc4f97bn, 0x3c717212f8a7525cn, 0x3febf641081e7536n, 0x3c8b7bd71628a9a1n,
  0x3fdf8e99e76abc97n, 0x3c59d950af2d00a3n, 0x3febd6ea310294f5n, 0x3c731bbcc88c109dn,
  0x3fdffdb628d2f57an, 0x3c6f4a992e905b6an, 0x3febb723fe630f32n, 0x3c772bd2452d0a39n,
  0x3fe0362939c69955n, 0xbc82d8cd78397b01n, 0x3feb96eeef58840en, 0x3c545a3cc78fade0n,
  0x3fe06d3686946e5bn, 0x3c83f5ae4538ff1bn, 0x3feb764b84b704c2n, 0xbc8f5848c21b389bn,
  0x3fe0a4021e9e1001n, 0xbc86f643a13914f6n, 0x3feb553a410c104en, 0x3c58ff7947027a16n,
  0x3fe0da8b26b5672en, 0xbc8a58def0bee909n, 0x3feb33bba89c8948n, 0x3c8ea6a51d1f6ca9n,
  0x3fe110d0c4b69c3bn, 0x3c8d918998809981n, 0x3feb11d04162a4c6n, 0x3c71dd561efbc0c2n,
  0x3fe146d21f8b7f82n, 0x3c7bf9535e2739a8n, 0x3feaef78930bd275n, 0xbc7f836279746f94n,
  0x3fe17c8e5f2eedb0n, 0x3c635e57102e2488n, 0x3feaccb526f69de5n, 0x3c88fb6a8dd6b6ccn,
  0x3fe1b204acb02fddn, 0xbc5f190c70cbb5ffn, 0x3feaa98688308913n, 0xbc0b83d607cd5070n,
  0x3fe1e7343236574cn, 0x3c722a3fa4f41d5an, 0x3fea85ed4373e02dn, 0x3c69be06385ec792n,
  0x3fe21c1c1b0394cfn, 0x3c5e5b324b23aa31n, 0x3fea61e9e72586afn, 0x3c858330e2fd453fn,
  0x3fe250bb93788bbbn, 0x3c7ea3d02457bccen, 0x3fea3d7d0352bdcfn, 0xbc868dbaeca19669n,
  0x3fe28511c917a067n, 0xbc801df1d9a16b70n, 0x3fea18a729aee445n, 0x3c395e25736c0358n,
  0x3fe2b91dea88421en, 0xbc8fa371db216ab0n, 0x3fe9f368ed912f85n, 0xbc81d200c5791606n,
  0x3fe2ecdf279a3082n, 0x3c8d3557e0e7e37en, 0x3fe9cdc2e3f25e5cn, 0x3c83f99112993f62n,
  0x3fe32054b148bc4fn, 0x3c8f6b42095a135bn, 0x3fe9a7b5a36a6514n, 0x3c8722cfcc9fa7a9n,
  0x3fe3537db9be0367n, 0x3c6b327e7af040f0n, 0x3fe98141c42e1310n, 0x3c8d1ff80488f08dn,
  0x3fe386597456282bn, 0xbc710fada93b07a8n, 0x3fe95a67e00cb1fdn, 0xbc80befda21f862dn,
  0x3fe3b8e715a2840an, 0xbc797653a7d2f07bn, 0x3fe93328926d9e92n, 0xbc8bb77003600cdan,
  0x3fe3eb25d36cd53an, 0xbc5be570e1570fc0n, 0x3fe90b84784ddaf7n, 0xbc70feb10ab93b87n,
  0x3fe41d14e4ba6790n, 0x3c84608fd287ecf5n, 0x3fe8e37c303d9ad1n, 0xbc6463a4b53d4bf8n,
  0x3fe44eb381cf386bn, 0xbc83ed6c1e6a5505n, 0x3fe8bb105a5dc900n, 0x3c8863e03e9474c1n,
  0x3fe48000e431159fn, 0xbc8b194a7463ed10n, 0x3fe89241985d871fn, 0x3c8c48d9c413ed84n,
  0x3fe4b0fc46aab761n, 0x3c20da05738cc59an, 0x3fe869108d77a6c6n, 0x3c7338ffe2bfe9ddn,
  0x3fe4e1a4e54ed51bn, 0xbc8a492f89b7c76an, 0x3fe83f7dde701ca0n, 0xbc4152cf609bc6e8n,
  0x3fe511f9fd7b351cn, 0xbc85c0e861c48831n, 0x3fe8158a31916d5dn, 0xbc6de8b90b8228den,
  0x3fe541facddbb724n, 0x3c7232c28520d391n, 0x3fe7eb362eaa1488n, 0x3c5a1d65a4a5959fn,
  0x3fe571a6966d59b3n, 0x3c5c843b4d0fb198n, 0x3fe7c0827f09e54fn, 0xbc6c73d6d72aee68n,
  0x3fe5a0fc98813a12n, 0xbc8d82e2b7d4227bn, 0x3fe7956fcd7f6543n, 0xbc8ab276e9d45ae4n,
  0x3fe5cffc16bf8f0dn, 0x3c896cb370eb578an, 0x3fe769fec655211fn, 0xbc6827d5cf8c68c5n,
  0x3fe5fea4552a9e57n, 0x3c80b6cef7ee20b7n, 0x3fe73e30174efba1n, 0xbc65d3ae3d94ad5fn,
  0x3fe62cf49921ac79n, 0xbc8edd9855b6241an, 0x3fe712046fa77678n, 0x3c8425b0a5029c81n,
  0x3fe65aec2963e755n, 0x3c8126f96b71053cn, 0x3fe6e57c800cf55en, 0x3c860286dedbd0a6n,
  0x3fe6888a4e134b2fn, 0xbc86b7d37644d5e6n, 0x3fe6b898fa9efb5dn, 0x3c715ac786ccf4b2n,
  0x3fe6b5ce50b7821an, 0xbc65d5158f702e0fn, 0x3fe68b5a92eb6253n, 0xbc89a91ad985f89cn,
  0x3fe6e2b77c40bde1n, 0xbc70e729857fad53n, 0x3fe65dc1fdeb8cban, 0xbc597c1b47337c77n,
  0x3fe70f451d0a8c40n, 0x3c697ede3885770dn, 0x3fe62fcff20191c7n, 0x3c6d9143895756efn,
  0x3fe73b7680dea578n, 0xbc72248306dc12a2n, 0x3fe6018526f563dfn, 0x3c846ca5e0e432d0n,
  0x3fe7674af6f7b524n, 0x3c7e9d3f94ac84a8n, 0x3fe5d2e255f1f17an, 0x3c80314104c8892bn,
  0x3fe792c1d0041d52n, 0xbc8abf05eeb354ebn, 0x3fe5a3e839824077n, 0x3c8428aa2759be62n,
  0x3fe7bdda5e28b3c2n, 0x3c4ad1197ccd0393n, 0x3fe574978d8e83f2n, 0x3c8f4714af282d23n,
  0x3fe7e893f5037959n, 0x3c80eefbaa650c4cn, 0x3fe544f10f592ca5n, 0xbc8e7ae8e6c7a62fn,
  0x3fe812ede9ae4ba4n, 0xbc87830adf402ddan, 0x3fe514f57d7bf3dan, 0x3c747a108073c259n,
];

const sincostab = new Float64Array(TABLE_BITS.length);
{
  const view = new DataView(sincostab.buffer);
  TABLE_BITS.forEach((bits, i) => view.setBigUint64(8 * i, bits, true));
}

const s1 = -0.16666666666666666;  // -0x1.5555555555555p-3
const s2 = 0.008333333333332329;  // 0x1.1111111110ECEp-7
const s3 = -0.00019841269834414642;  // -0x1.A01A019DB08B8p-13
const s4 = 2.755729806860771e-06;  // 0x1.71DE27B9A7ED9p-19
const s5 = -2.5022014848318398e-08;  // -0x1.ADDFFC2FCDF59p-26
const big = 52776558133248.0;  // 0x1.8000000000000p45
const hp0 = 1.5707963267948966;  // 0x1.921FB54442D18p0
const hp1 = 6.123233995736766e-17;  // 0x1.1A62633145C07p-54
const mp1 = 1.5707963407039642;  // 0x1.921FB58000000p0
const mp2 = -1.3909067564377153e-08;  // -0x1.DDE973C000000p-27
const pp3 = -4.97899623147991e-17;  // -0x1.CB3B398000000p-55
const pp4 = -1.9034889620193266e-25;  // -0x1.d747f23e32ed7p-83
const hpinv = 0.6366197723675814;  // 0x1.45F306DC9C883p-1
const toint = 6755399441055744.0;  // 0x1.8000000000000p52

const sn3 = -1.66666666666664880952546298448555E-01;
const sn5 = 8.33333214285722277379541354343671E-03;
const cs2 = 4.99999999999999999999950396842453E-01;
const cs4 = -4.16666666666664434524222570944589E-02;
const cs6 = 1.38888874007937613028114285595617E-03;

// mynumber: the two 32-bit halves of a double, as signed int4.
const word = new DataView(new ArrayBuffer(8));
function lowHalf(x) {
  word.setFloat64(0, x, true);
  return word.getInt32(0, true);
}
function highHalf(x) {
  word.setFloat64(0, x, true);
  return word.getInt32(4, true);
}
function copysign(x, y) {
  return (y < 0 || Object.is(y, -0)) ? -Math.abs(x) : Math.abs(x);
}

function taylorSin(xx, x, dx) {
  const polynomial = (((s5 * xx + s4) * xx + s3) * xx + s2) * xx + s1;
  const t = ((polynomial * x - 0.5 * dx) * xx + dx);
  return x + t;
}

function doCos(x, dx) {
  if (x < 0) dx = -dx;
  const u = big + Math.abs(x);
  x = Math.abs(x) - (u - big) + dx;
  const xx = x * x;
  const s = x + x * xx * (sn3 + xx * sn5);
  const c = xx * (cs2 + xx * (cs4 + xx * cs6));
  const k = lowHalf(u) << 2;
  const sn = sincostab[k], ssn = sincostab[k + 1];
  const cs = sincostab[k + 2], ccs = sincostab[k + 3];
  const cor = (ccs - s * ssn - cs * c) - sn * s;
  return cs + cor;
}

function doSin(x, dx) {
  const xold = x;
  if (Math.abs(x) < 0.126) return taylorSin(x * x, x, dx);
  if (x <= 0) dx = -dx;
  const u = big + Math.abs(x);
  x = Math.abs(x) - (u - big);
  const xx = x * x;
  const s = x + (dx + x * xx * (sn3 + xx * sn5));
  const c = x * dx + xx * (cs2 + xx * (cs4 + xx * cs6));
  const k = lowHalf(u) << 2;
  const sn = sincostab[k], ssn = sincostab[k + 1];
  const cs = sincostab[k + 2], ccs = sincostab[k + 3];
  const cor = (ssn + s * ccs - sn * c) + cs * s;
  return copysign(sn + cor, xold);
}

// Returns [n, a, da]: the quadrant and x reduced to within pi/2, for
// |x| < 105414350.
function reduceSincos(x) {
  const t = (x * hpinv + toint);
  const xn = t - toint;
  const y = (x - xn * mp1) - xn * mp2;
  const n = lowHalf(t) & 3;
  let t1 = xn * pp3;
  const t2 = y - t1;
  let db = (y - t2) - t1;
  t1 = xn * pp4;
  const b = t2 - t1;
  db += (t2 - b) - t1;
  return [n, b, db];
}

function doSincos(a, da, n) {
  const retval = (n & 1) ? doCos(a, da) : doSin(a, da);
  return (n & 2) ? -retval : retval;
}

// Above this the C code switches to __branred, not ported: refuse.
function range(x) {
  const k = 0x7fffffff & highHalf(x);
  if (k >= 0x419921FB) throw new RangeError('|x| >= 105414350: not ported');
  return k;
}

export function sin(x) {
  const k = range(x);
  if (k < 0x3e500000) return x;
  if (k < 0x3feb6000) return doSin(x, 0);
  if (k < 0x400368fd) return copysign(doCos(hp0 - Math.abs(x), hp1), x);
  const [n, a, da] = reduceSincos(x);
  return doSincos(a, da, n);
}

export function cos(x) {
  const k = range(x);
  if (k < 0x3e400000) return 1.0;
  if (k < 0x3feb6000) return doCos(x, 0);
  if (k < 0x400368fd) {
    const y = hp0 - Math.abs(x);
    const a = y + hp1;
    const da = (y - a) + hp1;
    return doSin(a, da);
  }
  const [n, a, da] = reduceSincos(x);
  return doSincos(a, da, n + 1);
}
