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


Subdivision Surfaces
-----------------------------

.. contents::
   :local:
   :backlinks: none

----

.. Introduction

はじめに
========

.. The most common way to model complex smooth surfaces is by using a patchwork of
   bicubic patches such as BSplines or NURBS.

滑らかなサーフェスをモデリングするもっとも一般的な方法は、BスプラインやNURBSなどの
双３次曲面をつなぎ合わせることです。

.. image:: images/torus.png
   :align: center
   :height: 200

.. However, while they do provide a reliable smooth limit surface definition,
   bi-cubic patch surfaces are limited to 2-dimensional topologies, which only
   describe a very small fraction of real-world shapes. This fundamental
   parametric limitation requires authoring tools to implement at least the
   following functionalities:

これらはしっかりした滑らかな極限サーフェスを得られますが、双３次サーフェスは
トポロジが２次元に限定されるため現実世界のシェイプのごく一部しか記述することができません。
この根本的なパラメータ制約により、オーサリングツールは少なくとも次の機能を持つ必要があります。

..     - smooth trimming
..     - seams stitching

   - スムーズトリミング
   - シーム（繋ぎ目）スティッチ

.. Both trimming and stitching need to guarantee the smoothness of the model both
   spatially and temporally as the model is animated. Attempting to meet these
   requirements introduces a lot of expensive computations and complexity.

ただしトリミングもスティッチングもモデルの滑らかさを保つ必要があり、またそれがアニメーションする
際にも滑らかであり続けなければなりません。これには複雑で負荷の高い計算が必要になってしまいます。

.. Subdivision surfaces on the other hand can represent arbitrary topologies, and
   therefore are not constrained by these difficulties.

一方サブディビジョンサーフェスはあらゆるトポロジを表現することが出来るため、上記のような
難しさにとらわれることがありません。

----

.. Arbitrary Topology

任意のトポロジ
==================

.. A subdivision surface, like  a parametric surface, is described by its control
   mesh of points. The surface itself can approximate or interpolate this control
   mesh while being piecewise smooth. But where polygonal surfaces require large
   numbers of data points to approximate being smooth, a subdivision surface is
   smooth - meaning that polygonal artifacts are never present, no matter how the
   surface animates or how closely it is viewed.

サブディビジョンサーフェスはその他のパラメトリックサーフェスと同様に、コントロール
メッシュのポイントで記述されます。サーフェスそれ自体は部分的にスムーズになるように
近似したり補間したりできるのですが、ポリゴンサーフェスを滑らかにするには膨大な量の
頂点が必要になるのに対し、サブディビジョンサーフェスは常に滑らか、つまりどのように
動いてもどれだけ近づいてもポリゴンのアーティファクトは発生しません。

.. Ordinary cubic B-spline surfaces are rectangular grids of tensor-product
   patches. Subdivision surfaces generalize these to control grids with arbitrary
   connectivity.

通常の３次Bスプラインサーフェスは格子状のテンソル積パッチです。サブディビジョンサーフェスは
これを任意の接続形状のコントロールグリッドに一般化したものになります。

.. raw:: html

    <center>
      <p align="center">
        <IMG src="images/tetra.0.png" style="width: 20%;">
        <IMG src="images/tetra.1.png" style="width: 20%;">
        <IMG src="images/tetra.2.png" style="width: 20%;">
        <IMG src="images/tetra.3.png" style="width: 20%;">
      </p>
    </center>

----

.. Uniform Subdivision

ユニフォーム（均一）サブディビジョン
====================================

.. Applies a uniform refinement scheme to the coarse faces of a mesh.
   The mesh converges closer to the limit surface with each iteration of the algorithm.

コントロールメッシュを均一に分割していきます。処理を繰り返すことでメッシュは極限サーフェスに
収束していきます。

.. image:: images/uniform.gif
   :align: center
   :width: 300
   :target: images/uniform.gif

----

.. Feature Adaptive Subdivision

フィーチャーアダプティブ（特徴適応型）サブディビジョン
==============================================================

.. Applies a progressive refinement strategy to isolate irregular features.
   The resulting vertices can be assembled into bi-cubic patches defining the limit surface.

非正則な特徴を分離するような段階的な分割を行います。得られた頂点を使って極限サーフェスを
直接定義する双三次サーフェスを得る事ができます。

.. image:: images/adaptive.gif
   :align: center
   :width: 300
   :target: images/adaptive.gif

----

.. Uniform or Adaptive ?

均一分割か、適応型分割か
========================

.. Feature adaptive refinement can be much more economical in terms of time and memory use,
   but the best method to use depends on application needs.

特徴適応型分割は速度面でもメモリ消費面でも有利ですが、アプリケーションの用途によって選んでください。

.. The following table identifies several factors to consider:

以下の表に検討すべき要素を列挙しています。

+-------------------------------------------------------+--------------------------------------------------------+
| Uniform                                               | Feature Adaptive                                       |
+=======================================================+========================================================+
|                                                       |                                                        |
| * Exponential geometry growth                         | * Geometry growth close to linear and occuring only in |
|                                                       |   the neighborhood of isolated topological features    |
|                                                       |                                                        |
+-------------------------------------------------------+--------------------------------------------------------+
|                                                       |                                                        |
| * Current implementation only produces bi-linear      | * Current implementation only produces bi-cubic        |
|   patches for uniform refinement                      |   patches for feature adaptive refinement              |
|                                                       |                                                        |
+-------------------------------------------------------+--------------------------------------------------------+
|                                                       |                                                        |
| * All face-varying interpolation rules supported at   | * Currently, only bi-linear face-varying interpolation |
|   refined vertex locations                            |   is supported for bi-cubic patches                    |
|                                                       |                                                        |
+-------------------------------------------------------+--------------------------------------------------------+

|
|
|

.. container:: notebox

   **Release Notes (3.0.0)**

   * Full support for bi-cubic face-varying interpolation is a significant
     feature which will be supported in future releases.

   * Feature adaptive refinement for the Loop subdivision scheme is
     expected to be supported in future releases.

----

.. Boundary Interpolation Rules

境界補間ルール
============================

.. Boundary interpolation rules control how boundary edges and vertices are interpolated.

   The following rule sets can be applied to vertex data interpolation:

境界補間ルールとは、境界エッジと頂点をどのように補間するか制御する方法です。

以下のルールが頂点の補間に適用可能です:

.. +----------------------------------+----------------------------------------------------------+
   | Mode                             | Behavior                                                 |
   +==================================+==========================================================+
   | **VTX_BOUNDARY_NONE**            | No boundary edge interpolation should occur; instead     |
   |                                  | boundary faces are tagged as holes so that the boundary  |
   |                                  | edge-chain continues to support the adjacent interior    |
   |                                  | faces but is not considered to be part of the refined    |
   |                                  | surface                                                  |
   +----------------------------------+----------------------------------------------------------+
   | **VTX_BOUNDARY_EDGE_ONLY**       | All the boundary edge-chains are sharp creases; boundary |
   |                                  | vertices are not affected                                |
   +----------------------------------+----------------------------------------------------------+
   | **VTX_BOUNDARY_EDGE_AND_CORNER** | All the boundary edge-chains are sharp creases and       |
   |                                  | boundary vertices with exactly one incident face are     |
   |                                  | sharp corners                                            |
   +----------------------------------+----------------------------------------------------------+

+----------------------------------+----------------------------------------------------------+
| モード                           | 挙動                                                     |
+==================================+==========================================================+
| **VTX_BOUNDARY_NONE**            | 何もしない;境界になる面はホールとして扱われます。        |
|                                  | 内側の面の連続性のために使われますが、それ自体は極限     |
|                                  | サーフェスに含まれません。                               |
+----------------------------------+----------------------------------------------------------+
| **VTX_BOUNDARY_EDGE_ONLY**       | すべてのバウンダリエッジをシャープクリースとする         |
|                                  |                                                          |
+----------------------------------+----------------------------------------------------------+
| **VTX_BOUNDARY_EDGE_AND_CORNER** | すべてのバウンダリエッジをシャープクリースとし、面を一つ |
|                                  | だけもつバウンダリ頂点をシャープコーナーとする           |
+----------------------------------+----------------------------------------------------------+


.. On a grid example:

グリッドの例:

.. image:: images/vertex_boundary.png
   :align: center
   :target: images/vertex_boundary.png


----

.. Face-Varying Interpolation Rules

面変化(Face Varying) 補間ルール
================================

.. Face-varying data is used when discontinuities are required in the data over the
   surface -- mostly commonly the seams between disjoint UV regions.
   Face-varying data can follow the same interpolation behavior as vertex data, or it
   can be constrained to interpolate linearly around selective features from corners,
   boundaries, or the entire interior of the mesh.

面変化(Face-varying)データはサーフェス上で切れ目があるデータ、主に切れ目のあるUV
などに使われます。面変化データは頂点データの補間と同じ規則に従うか、あるいはコーナー、
境界、内部をそれぞれ直線的に補間するか選ぶ事ができます。

.. The following rules can be applied to face-varying data interpolation -- the
   ordering here applying progressively more linear constraints:

面変化データの補間には以下の規則が適用可能です。下の表は直線的な補間になる順にならんでいます。

+--------------------------------+-------------------------------------------------------------+
| Mode                           | Behavior                                                    |
+================================+=============================================================+
| **FVAR_LINEAR_NONE**           | smooth everywhere the mesh is smooth                        |
+--------------------------------+-------------------------------------------------------------+
| **FVAR_LINEAR_CORNERS_ONLY**   | sharpen (linearly interpolate) corners only                 |
+--------------------------------+-------------------------------------------------------------+
| **FVAR_LINEAR_CORNERS_PLUS1**  | CORNERS_ONLY + sharpening of junctions of 3 or more regions |
+--------------------------------+-------------------------------------------------------------+
| **FVAR_LINEAR_CORNERS_PLUS2**  | CORNERS_PLUS1 + sharpening of darts and concave corners     |
+--------------------------------+-------------------------------------------------------------+
| **FVAR_LINEAR_BOUNDARIES**     | linear interpolation along all boundary edges and corners   |
+--------------------------------+-------------------------------------------------------------+
| **FVAR_LINEAR_ALL**            | linear interpolation everywhere (boundaries and interior)   |
+--------------------------------+-------------------------------------------------------------+

.. These rules cannot make the interpolation of the face-varying data smoother than
   that of the vertices.  The presence of sharp features of the mesh created by
   sharpness values, boundary interpolation rules, or the subdivision scheme itself
   (e.g. Bilinear) take precedence.

どの面変化補間ルールを使っても、ベースになる頂点より面変化データを滑らかに補間することはできません。
メッシュのシャープな特徴はシャープネス値、境界補間ルール、スキーム自体が優先します。

..  All face-varying interpolation modes illustrated in UV space using the
   catmark_fvar_bound1 regression shape -- a simple 4x4 grid of quads segmented
   into three UV regions (their control point locations implied by interpolation
   in the FVAR_LINEAR_ALL case):

catmark_fvar_bound1 テストシェイプをすべての規則で分割した UV 値の例。このシェイプは
単純な 4x4 グリッドの四角形ですが、３つの UV 領域に分かれています（コントロールポイント
の場所は FVAR_LINEAR_ALL の様子でわかります）:

.. image:: images/fvar_boundaries.png
   :align: center
   :target: images/fvar_boundaries.png


----

.. Semi-Sharp Creases

セミシャープクリース
====================

.. It is possible to modify the subdivision rules to create piecewise smooth
   surfaces containing infinitely sharp features such as creases and corners. As a
   special case, surfaces can be made to interpolate their boundaries by tagging
   their boundary edges as sharp.

サブディビジョン規則を一部変更して部分的に滑らかなサーフェスにクリースやコーナー
などの完全にシャープな特徴を作ることができます。特別なケースとしては境界エッジを
シャープとタグ付けし、境界を補間することができます。

.. However, we've recognized that real world surfaces never really have infinitely
   sharp edges, especially when viewed sufficiently close. To this end, we've
   added the notion of semi-sharp creases, i.e. rounded creases of controllable
   sharpness. These allow you to create features that are more akin to fillets and
   blends. As you tag edges and edge chains as creases, you also supply a
   sharpness value that ranges from 0-10, with sharpness values >=10 treated as
   infinitely sharp.

しかし、現実世界のサーフェスは完全にシャープなエッジというのはありません。このため
セミシャープクリースという表現を導入しました。つまり、丸みを帯びた制御可能な
シャープネスをもつクリースです。これによりフィレットやブレンドのような特徴を
作ることができます。エッジや連結エッジをクリースでタグ付ける際に、シャープネスの値を
0〜10の範囲で与える事ができます。10 以上のシャープネスは完全シャープとして扱います。

.. It should be noted that infinitely sharp creases are really tangent
   discontinuities in the surface, implying that the geometric normals are also
   discontinuous there. Therefore, displacing along the normal will likely tear
   apart the surface along the crease. If you really want to displace a surface at
   a crease, it may be better to make the crease semi-sharp.

完全シャープクリースでは接線が不連続になり、法線ベクトルも不連続になることに
注意してください。このため法線方向へのディスプレイスメントはクリースにそって
分離してしまいます。クリース部分でディスプレイスメントが必要な場合はセミシャープ
クリースをつけるようにしてください。

.. image:: images/gtruck.png
   :align: center
   :height: 300
   :target: images/gtruck.png

----

.. Chaikin Rule

チャイキン則
============

.. Chaikin's curve subdivision algorithm improves the appearance of multi-edge
   semi-sharp creases with varying weights. The Chaikin rule interpolates the
   sharpness of incident edges.

チャイキンのカーブ分割アルゴリズムを使って複数エッジに渡るセミシャープ
クリースの見た目を改善することができます。チャイキン則はシャープネスを
隣接エッジで補間します。

.. +---------------------+---------------------------------------------+
   | Mode                | Behavior                                    |
   +=====================+=============================================+
   | **CREASE_UNIFORM**  | Apply regular semi-sharp crease rules       |
   +---------------------+---------------------------------------------+
   | **CREASE_CHAIKIN**  | Apply "Chaikin" semi-sharp crease rules     |
   +---------------------+---------------------------------------------+

+---------------------+------------------------------------------------------+
| モード              | 挙動                                                 |
+=====================+======================================================+
| **CREASE_UNIFORM**  | 通常のセミシャープクリース則を適用                   |
+---------------------+------------------------------------------------------+
| **CREASE_CHAIKIN**  | セミシャープクリース則にチャイキンアルゴリズムを適用 |
+---------------------+------------------------------------------------------+


.. Example of contiguous semi-sharp creases interpolation:
連続するセミシャープクリースの補間例:

.. image:: images/chaikin.png
   :align: center
   :target: images/chaikin.png

----

.. "Triangle Subdivision" Rule

"三角形サブディビジョン"規則
============================

.. The triangle subdivision rule is a rule added to the Catmull-Clark scheme that
   can be applied to all triangular faces; this rule was empirically determined to
   make triangles subdivide more smoothly. However, this rule breaks the nice
   property that two separate meshes can be joined seamlessly by overlapping their
   boundaries; i.e. when there are triangles at either boundary, it is impossible
   to join the meshes seamlessly

三角形サブディビジョン規則は Catmull-Clark 分割のオプションで、三角形フェイスの
分割に適用することができます。この規則は経験的に決められたもので、三角形の分割を
滑らかにすることができます。ただし、この規則は２つのメッシュの境界を重ねる
ことでシームレスに接続できるという便利な特性をなくしてしまいますので注意してください。

.. +---------------------+---------------------------------------------+
   | Mode                | Behavior                                    |
   +=====================+=============================================+
   | **TRI_SUB_CATMARK** | Default Catmark scheme weights              |
   +---------------------+---------------------------------------------+
   | **TRI_SUB_SMOOTH**  | "Smooth triangle" weights                   |
   +---------------------+---------------------------------------------+

+---------------------+---------------------------------------------+
| モード              | 挙動                                        |
+=====================+=============================================+
| **TRI_SUB_CATMARK** | 標準の Catmark 重みづけ                     |
+---------------------+---------------------------------------------+
| **TRI_SUB_SMOOTH**  | "スムーズ三角形" 重みを適用                 |
+---------------------+---------------------------------------------+

.. Cylinder example :

シリンダーの例 :

.. image:: images/smoothtriangles.png
   :align: center
   :height: 300
   :target: images/smoothtriangles.png


----

.. Manifold vs Non-Manifold Geometry

多様体と非多様体ジオメトリ
=================================

.. Continuous limit surfaces generally require that the topology be a
   two-dimensional manifold for the limit surface to be unambiguous.  It is
   possible (and sometimes useful, if only temporarily) to model non-manifold
   geometry and so create surfaces whose limit is not as well-defined.

   The following examples show typical cases of non-manifold topological
   configurations.

極限サーフェスを連続に一意に作るためには一般的にはトポロジが二次元多様体で
ある必要があります。時として一時的に非多様体ジオメトリをモデリングするのが
便利なこともありますが、その場合には極限サーフェスは定義されません。

以下の例は非多様体ジオメトリを構成する典型的な例です。


----

.. Non-Manifold Fan

非多様体の羽根型形状
********************

.. This "fan" configuration shows an edge shared by 3 distinct faces.

次の「羽根型（矢尻型）」のジオメトリは一つのエッジを３つの面で共有しています。

.. image:: images/nonmanifold_fan.png
   :align: center
   :target: images/nonmanifold_fan.png

.. With this configuration, it is unclear which face should contribute to the
   limit surface (assuming it is singular) as three of them share the same edge.
   Fan configurations are not limited to three incident faces: any configuration
   where an edge is shared by more than two faces incurs the same problem.

この構造だとどの面が極限サーフェスを構成するのかを決めることができません。
これは３面の場合にかぎらず、３面以上の面がエッジを共有するときには同じ
問題が発生します。

.. These and other regions involving non-manifold edges are dealt with by
   considering regions that are "locally manifold".  Rather than a single limit
   surface through this problematic edge with its many incident faces, the edge
   locally partitions a single limit surface into more than one.  So each of the
   three faces here will have their own (locally manifold) limit surface -- all
   of which meet at the shared edge.

非多様体エッジは、「部分的に多様体」な領域を考慮して取り扱われます。複数の面を
含む問題があるエッジを補間して通過する極限サーフェスのかわりに、そのエッジで複数の極限
サーフェスを幾つかに分割します。こうして面がそれぞれ極限サーフェスを持ち
（部分的に多様体）、エッジでつながる、というような処理になります。


----

.. Non-Manifold Disconnected Vertex

非多様体として扱われる独立頂点
********************************

.. A vertex is disconnected from any edge and face.

どのエッジ・面にも属していない頂点の場合です。

.. image:: images/nonmanifold_vert.png
   :align: center
   :target: images/nonmanifold_vert.png

.. This case is fairly trivial: there is a very clear limit surface for the four
   vertices and the face they define, but no possible way to exact a limit surface
   from the disconnected vertex.

このケースは簡単です：４つの頂点とその面にはもちろん極限サーフェスがありますが、
つながっていない頂点にはありません。

.. While the vertex does not contribute to any
   limit surface, it may not be completely irrelevant though.  Such vertices may
   be worth retaining during subdivision (if for no other reason than to preserve
   certain vertex ordering) and simply ignored when it comes time to consider
   the limit surface.

この頂点はどの極限サーフェスにも寄与しませんが、まったく無関係というわけでも
ありません。これらの頂点をサブディビジョン処理の間保持しておきたい場合も
あります（頂点順序を維持したい場合）。これらは単純に極限サーフェスを求める
ときには無視されます。


