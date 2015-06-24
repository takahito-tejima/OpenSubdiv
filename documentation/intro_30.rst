..
     Copyright 2013 Pixar

     Licensed under the Apache License, Version 2.0 (the "Apache License")
     with the following modification; you may not use this file except in
     compliance with the Apache License and the following modification to it:
     Section 6. Trademarks. is deleted and replaced with:

     6. Trademarks. This License does not grant permission to use the trade
        names, trademarks, service marks, or product names of the Licensor
        and its affiliates, except as required to comply with Section 4(c) of
        the License and to reproduce the content of the NOTICE file.

     You may obtain a copy of the Apache License at

         http://www.apache.org/licenses/LICENSE-2.0

     Unless required by applicable law or agreed to in writing, software
     distributed under the Apache License with the above modification is
     distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY
     KIND, either express or implied. See the Apache License for the specific
     language governing permissions and limitations under the Apache License.


Overview of Release 3.0
-----------------------

.. contents::
   :local:
   :backlinks: none

----

.. Release 3.0

バージョン 3.0
==============

.. OpenSubdiv 3.0 represents a landmark release, with profound changes to the core
   algorithms, simplified APIs, and streamlined GPU execution. Providing faster,
   more efficient, and more flexible subdivision code remains our principal goal.
   To achieve this, OpenSubdiv 3.0 introduces many improvements that constitute
   a fairly radical departure from previous versions.

OpenSubdiv 3.0 は根本的に変更されたコアアルゴリズム、簡単になったAPI、効率的に
なったGPUカーネルなど多岐にわたる大きな変更を行った新バージョンになります。
より高速、効率的でフレキシブルなサブディビジョンサーフェスのライブラリを提供する
ことが私達の主な目的であることは変わっていませんが、そのために OpenSubdiv 3.0 は
数多くの改良が施され以前のバージョンから大きく変わりました。

.. This document highlights some of the major changes that have gone in to the 3.0
   release.
 このドキュメントは 3.0 リリースに伴う大きな変更点を紹介します。

----

.. Subdivision Core (Sdc)

サブディビジョン・コア (Sdc)
****************************

.. In consideration of past, present and future topological representations,
   all low-level details fundamental to subdivision and the specific subdivision
   schemes have been factored into a new low-level layer called **Sdc**
   (SubDivision Core).  This layer encapsulates the full set of applicable
   options, the formulae required to support semi-sharp creasing, the formulae
   for the refinement masks of each subdivision scheme, etc.  As initially
   conceived, its goal was often expressed as "separating the math from the mesh".

過去・現在・未来に使われた、使われるであろう様々なトポロジ表現を考慮して、
サブディジョンの根本的な規則と各種サブディビジョンスキームは新しい低レベルレイヤー **Sdc**
（サブディビジョンコア） にまとめられました。このレイヤーは利用可能な
オプション、例えばセミシャープクリースをサポートするための計算式、各種サブディビジョン
スキームの分割重みなどを集約しています。これは「計算方法をメッシュから分離」する
のが目的です。

.. Sdc provides the low-level nuts and bolts to provide a subdivision
   implementation consistent with OpenSubdiv. It is used by OpenSubdiv's
   libraries and may also be useful in providing an existing client's
   implementation with the details necessary to make that implementation
   consistent with OpenSubdiv.

Sdc は OpenSubdiv で一貫性のあるサブディビジョンアルゴリズムを実装するための
低レベルな機構です。OpenSubdiv 自身で使われる他、OpenSubdiv と同じになる実装を
クライアントアプリケーションで実装するためにも使うことができます。

----

.. Topology and Refinement

トポロジーと分割
***********************

.. OpenSubdiv 3.0 introduces a new *intermediate* internal topological
   representation named **Vtr** (Vectorized Topology Representation).
   Compared to the Hbr library used in previous versions, Vtr is much more
   efficient for the kinds of topological analysis required by Far and is more
   flexible.  While Hbr is no longer used by OpenSubdiv, it will remain in
   the source distribution for legacy and regression purposes.

OpenSubdiv 3.0 は新たに *一時的に* 内部で利用するトポロジ表現、**Vtr**
(ベクトル化トポロジ表現) を導入しました。以前のバージョンで使っていた Hbr
に比べると、Vtr は Far で利用するトポロジ解析をより効率的に、柔軟に行うことが
できます。Hbr は OpenSubdiv では利用しなくなりますが、レガシー互換や
テストのためにライブラリ内に残してあります。

.. **Faster Subdivision**

**高速なサブディビジョン**

..  A major focus of the 3.0 release is performance, and the improvement to
    the initial refinement of a mesh required for topological analysis is close
    to an order magnitude; often much more for uniform, but less for adaptive.

 バージョン 3.0 はパフォーマンス向上に注力しており、最初のメッシュトポロジ解析
 にかかる時間は10倍近く速くなっています（均一分割でそれ以上になることもありますが、
 適応分割はもともと効率的なため速度向上は限られます）

.. **Supporting for Non-manifold Topology**

**非多様体トポロジのサポート**

..  With topology conversion no longer constrained by Hbr, OpenSubdiv is no
    longer restricted to meshes of manifold topology.  With one exception
    (non-triangles with Loop subdivision), any set of faces and vertices that can
    be represented in common container formats such as Obj or Alembic can be
    represented and subdivided.  With future efforts to bring the functionality
    for the Loop scheme up to par with Catmark, that last remaining topological
    restriction will be removed.

トポロジ変換でHbr を使わなくなったため、OpenSubdiv は多様体メッシュ以外にも
使えるようになりました。一つの例外（ループ分割での非三角形）を除き、一般的な
ファイルフォーマット(obj や Alembic）で表現できるどんなフェイスや頂点の構成
でも分割が可能です。ループ分割のサポートは今後充実する予定で、その例外もいずれ
なくなります。

.. **Simpler Conversion of Topology**

**トポロジの簡単な変換**

..  Several entry-points are now available for client topology, rather than the
    single incremental assembly of an HbrMesh that previously existed.  The new
    topological relationships can be populated using either a high-level interface
    where simplicity has been emphasized, or a more complex lower-level interface
    for enhanced efficiency.

以前のように HbrMesh を構築するのではなく、いくつかの方法でクライアントアプリケーション
からトポロジを与えることが可能です。単純に使える高レベルインターフェイス、あるいは
より効率を求めるなら複雑な低レベルインタフェースを使ってトポロジを構築することが
できます。

.. **Face Varying Topology**

**面変化(Face Varying)トポロジ**

..  Previously, face-varying data was assigned by value to the vertex for each
    face, and whether or not the set of values around a vertex was continuous was
    determined by comparing these values later. In some cases this could result
    in two values that were not meant to be shared being "welded" together.

以前は面変化データは各面の各点について割り当てられ、値を比較することで繋ぎ目を
検出していました。この場合、意図せず２つの値が共有されてしまう問題がありました。

..  Face-varying data is now specified topologically:  just as the vertex topology
    is defined from a set of **vertices** and integer references (indices) to
    these **vertices** for the corner of each face, face-varying topology is
    defined from a set of **values** and integer references (indices) to these
    **values** for the corner of each face. So if values are to be considered
    distinct around a vertex, they are given distinct indices and no comparison
    of any data is ever performed.  Note that the number of **vertices** and
    **values** will typically differ, but since indices are assigned to the
    corners of all faces for both, the total number of indices assigned to all
    faces will be the same.

3.0 からは面変化データはトポロジで与えることになります。これは頂点のトポロジ
が**頂点**集合と整数の頂点への参照(インデックス)で与えられるのと同じように、
面変化データも**値**の集合とそれへのインデクス、で定義されるようになります。
もしある頂点で面ごとに異なる値になる場合は、インデクスが変わる、ということ
になります。このとき **頂点** の数と **面変化データの値** の数は異なる場合が
ありますが、インデクスの総数は同じになる、ということに注意してください。

..  This ensures that OpenSubdiv's face-varying topology matches what is often
    specified in common geometry container formats like Obj, Alembic and USD.
    Multiple "channels" of face-varying data can be defined and each is
    topologically independent of the others.

面変化データトポロジは主要なファイルフォーマットである obj, Alembic, USD で
使われている表現方法と同じになります。複数の面変化データのチャンネルはそれぞれ
のトポロジで独立に表されます。

----

.. Limit Properties and Patches

リミットプロパティとパッチ
****************************

.. A fundamental goal of OpenSubdiv is to provide an accurate and reliable
   representation of the limit surface.  Improvements have been made both to the
   properties (positions and tangents) at discrete points in the subdivision
   hierarchy, as well as to the representations of patches used for the
   continuous limit surface between them.

OpenSubdiv の根本的な目標は、正確かつ信頼性のあるリミットサーフェス表現を提供する
ことにあります。サブディジョン階層中のコントロール点におけるリミットサーフェスの値
（座標、接線）の計算、およびパッチを使った任意の場所でのリミットサーフェスの値の
計算に様々な改良が行われました。

.. **Removed Fixed Valence Tables**

**固定長の頂点価テーブルの廃止**

 .. Limit properties of extra-ordinary vertices are computed for arbitrary
    valence and new patch types no longer rely on small table sizes.  All tables
    that restricted the valence of a vertex to some relatively small table size
    have now been removed.

特異頂点でのリミット値はどんな価数の頂点でも計算でき、新しい種類のパッチは
組み込みの固定長テーブルを使わなくなりました。頂点価数を制約していたテーブルは
削除されました。

..  The only restriction on valence that exists is within the new topology
    representation, which restricts it to the size of an unsigned 16-bit integer
    (65,535).  This limit could also be removed, by recompiling with a certain
    size changed from 16- to 32-bits, but doing so would increase the memory cost
    for all common cases.  We feel the 16-bit limit is a reasonable compromise.

新しいトポロジ表現での価数に関する唯一の制約は、符号なし16ビット整数(65,535)
の範囲になります。この制限は 32ビット整数にしてコンパイルすることで取り除くことが
できますが、ほとんどの場合メモリの無駄になってしまうため、16ビット整数は合理的で
あると考えています。

.. **Single Crease Patch**

**シングルクリースパッチ**

..  OpenSubdiv 3.0 newly implements efficient evaluation of semi-smooth
    creases(*) using single crease patches. With this optimization,
    high-order edge sharpness tags can be handled very efficiently for both
    computation time and memory consumption.

OpenSubdiv 3.0 は新たに効率的にセミシャープクリース(*)を評価できる
シングルクリースパッチを実装しました。この最適化により、高いシャープネスを
持つエッジを非常に高速かつ省メモリに効率的に取り扱うことができます。

 (*) Niessner et al., Efficient Evaluation of Semi-Smooth Creases in
 Catmull-Clark Subdivision Surfaces. Eurographics (Short Papers). 2012.
 `<http://research.microsoft.com/en-us/um/people/cloop/EG2012.pdf>`_

.. **New Irregular Patch Approximations**

**新たな非正則パッチの近似方法**

..  While "legacy" Gregory patch support is still available, we have introduced
    several new options for representing irregular patches: Legacy Gregory, fast
    Gregory Basis stencils, and BSpline patches. Gregory basis stencils provide
    the same high quality approximation of Legacy Gregory patches, but execute
    considerably faster with a simpler GPU representation. While BSpline patches
    are not as close an approximation as Gregory patches, they enable an entire
    adaptively refined mesh to be drawn with screen space tessellation via a
    single global shader configuration (Gregory Basis patches require one
    additional global shader configuration).

以前のバージョンのグレゴリーパッチもレガシーとして残してありますが、幾つか
新しく非正則パッチの表現に使う選択肢を追加しました。グレゴリー基底パッチ（新）、
Bスプライン基底パッチが新しく選択可能です。グレゴリー基底パッチは以前の
バージョンのグレゴリーパッチと同じ精度の近似を行いますが、パッチ頂点を
ステンシルテーブルで事前処理することで以前より高速でシンプルなGPU描画が
可能です。Bスプラインパッチはグレゴリー基底パッチより精度が落ちますが、
適応分割メッシ全体を単一のシェーダで描画することが可能になります（グレゴリー基底
パッチは正則パッチ以外に別のシェーダがひとつ必要です）

..  The new implementations of the GregoryBasis and BSpline approximations relax
    the previous max valence limit. Legacy Gregory patch still has a limitation
    of max valence (typically 24, depending on the hardware capability of
     GL_MAX_VARYING_VECTORS).

新しいグレゴリー基底パッチとBスプラインパッチによる非正則パッチでは、以前にあった
最大価数の制約がなくなります。レガシーグレゴリーパッチは依然として最大価数の
制約があります（通常24、ハードウェアの GL_MAX_VARYING_VECTORS に依存）

..  Users are still encouraged to use models with vertices of low valence for
    both improved model quality and performance.

ただし、モデリングの際には価数を低く抑えておくことがクオリティとパフォーマンスの
両面で重要であることは変わっていません。

----

.. Faster Evaluation and Display

高速な演算と描画
****************

.. OpenSubdiv 3.0 also introduces new data structures and algorithms that greatly
   enhance performance for the common case of repeated evaluation both on the
   CPU and GPU.

OpenSubdiv 3.0 は CPU・GPU どちらの場合でも、繰り返しサブディビジョンを行う
際のパフォーマンスを向上する新しいデータ構造とアルゴリズムを導入しました。

.. **Introducing Stencil Tables**

**ステンシルテーブル**

..  OpenSubdiv 3.0 replaces the serialized subdivision tables with factorized
    stencil tables. The SubdivisionTables class of earlier releases contained
    a large number of data inter-dependencies, which incurred penalties from
    fences or force additional kernel launches. Most of these dependencies have now
    been factorized away in the pre-computation stage, yielding *stencil tables*
    (Far::StencilTable) instead.

OpenSubdiv 3.0 は従来のサブディビジョンテーブルのかわりにステンシルテーブルを
使います。サブディビジョンテーブルクラスはデータに相互依存があり並列実行の際に
同期処理や別カーネルの実行が必要でした。これらの依存は事前計算で処理され、
ステンシルテーブルになります。

..  Stencils remove all data dependencies and simplify all the computations into a
    single trivial kernel. This simplification results in a faster pre-computation
    stage, faster execution on GPU, with less driver overhead. The new stencil
    tables Compute back-end is supported on all the same platforms as previous
    releases (except GCD).

ステンシルはデータ依存がなく単一のシンプルなカーネルで処理されます。これにより
事前計算、GPU実行ともに高速化され、ドライバのオーバヘッドも減りました。
ステンシルテーブルのバックエンドは以前のバージョンと同様のものがサポートされて
います（GCDを除く）

.. **Faster, Simpler GPU Kernels**

**高速、シンプルなGPUカーネル**

..  On the GPU side, the replacement of subdivision tables with stencils greatly
    reduces bottlenecks in compute, yielding as much as a 4x interpolation speed-up.
    At the same time, stencils reduce the complexity of interpolation to a single
    kernel launch per primitive, a critical improvement for mobile platforms.

GPU側では、サブディビジョンテーブルのかわりにステンシルを使うことでボトルネックが
解消され、4倍近くの高速化ができています。また複雑な補間がモデルごとに一回のカーネル
実行になることで、モバイルプラットフォームなどで大きな性能向上が得られます。

..  As a result of these changes, compute batching is now trivial, which in turn
    enabled API simplifications in the Osd layer.

結果として演算のバッチ実行は単純化され、Osd レイヤーのAPIがシンプルになりました。

.. **Unified Adaptive Shaders**

**統合パッチシェーダ**

..  Adaptive tessellation shader configurations have been greatly simplified. The
    number of shader configurations has been reduced from a combinatorial per-patch
    explosion down to a constant two global configurations. This massive improvement
    over the 2.x code base results in significantly faster load times and a reduced
    per-frame cost for adaptive drawing.

適応型テセレーションシェーダの構成は劇的に単純化されました。シェーダの数は
以前のパッチ構成の組み合わせ数から、１つあるいは２つに減りました。この大きな改良
により 2.x に比べてロード時間が激減し、描画コストも減ります。

..  Similar to compute kernel simplification, this shader simplification has
    resulted in additional simplifications in the Osd layer.

演算カーネルの単純化と同様に、シェーダの単純化により Osd レイヤーはよりシンプルに
なりました。s

----

.. Updated Source-Code Style

コードスタイルの更新
********************

.. OpenSubdiv 3.0 replaces naming prefixes with C++ namespaces for all API layers,
   bringing the source style more in line with contemporary specifications
   (mostly inspired from the `Google C++ Style Guide
   <http://google-styleguide.googlecode.com/svn/trunk/cppguide.xml>`__).

OpenSubdiv 3.0 から、識別子のプレフィックスがC++名前空間になりました。

.. The large-scale changes introduced in this release generally break compatibility
   with existing client-code. However, this gives us the opportunity to effect
   some much needed updates to our code-style guidelines and general conventions,
   throughout the entire OpenSubdiv code-base. We are hoping to drastically
   improve the quality, consistency and readability of the source code.

このリリースでの大幅な変更により既存のクライアントコードは互換性がなくなりましたが、
各所でコーディングスタイルの不一致を更新することができました。これにより品質、
一貫性、読みやすさなどが向上していると期待しています。

----

.. Documentation and Tutorials

ドキュメント／チュートリアル
****************************

.. The documentation has been reorganized and fleshed out. This release
   introduces a number of new `tutorials <tutorials.html>`__. The tutorials
   provide an easier entry point for learning the API than do the programs
   provided in examples. The examples provide more fleshed out solutions and are
   a good next step after the tutorials are mastered.

ドキュメントは一新されました。また本リリースより幾つかのチュートリアルが追加
されています。チュートリアルはサンプルプログラムより簡単に API を学ぶことができます。
チュートリアルの次はサンプルプログラムが参考になることかと思います。

----

Additional Resources
====================

.. Porting Guide

移植ガイド
*************

.. Please see the `Porting Guide <porting.html>`__ for help on how to port
.. existing code written for OpenSubdiv 2.x to the new 3.0 release.

OpenSubdiv 2.x で書かれた既存のコードを 3.0 に更新する方法は
`Porting Guide <porting.html>`__ をご覧ください。

----

.. Subdivision Compatibility

サブディビジョン互換性
*************************

.. The 3.0 release has made some minor changes to the subdivision specification
   and rules.  See `Subdivision Compatibility <compatibility.html>`__ for a
   complete list.

3.0 リリースはサブディビジョンの仕様に幾つか小変更を行っています。
`Subdivision Compatibility <compatibility.html>`__  をご覧ください。

