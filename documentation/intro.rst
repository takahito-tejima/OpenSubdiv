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
  

Introduction
------------

.. image:: images/geri.jpg 
   :width: 100%
   :align: center

.. contents::
   :local:
   :backlinks: none

----

.. Introduction

はじめに
============

.. OpenSubdiv is a set of open source libraries that implement high performance 
   subdivision surface (subdiv) evaluation on massively parallel CPU and GPU 
   architectures. This code path is optimized for drawing deforming surfaces with 
   static topology at interactive framerates. 
   OpenSubdiv は CPU や GPU の大規模並列アーキテクチャ上で高効率なサブディビジョン

サーフェス計算を行うオープンソースのライブラリです。静的なトポロジ上で変形する
サーフェスをインタラクティブなフレームレートで描画できるように最適化されています。
得られる極限サーフェスはピクサーの RenderMan 仕様と数値計算精度の範囲内で一致します。

.. OpenSubdiv is an API ready to be integrated into 3rd party digital
   content creation tools. It is **not** an application, nor a tool that can be 
   used directly to create digital assets.

OpenSubdiv はサードパーティのDCCツールに組み込まれることを想定した API ですが、
これ自体で直接アセットを作るためのアプリケーションやツールではありません。


----

.. Why Fast Subdivision ?
   
なぜ高速なサブディビジョンが必要なのか
=======================================

.. Subdivision surfaces are commonly used for final rendering of character shapes 
   for a smooth and controllable limit surfaces. However, subdivision surfaces in 
   interactive apps are typically drawn as their polygonal control hulls because of 
   performance. The polygonal control hull is an approximation that is offset from 
   the true limit surface. Looking at an approximation in the interactive app makes 
   it difficult to see exact contact, like fingers touching a potion bottle or hands 
   touching a cheek. It also makes it difficult to see poke-throughs in cloth simulation 
   if the skin and cloth are both approximations. This problem is particularly bad when 
   one character is much larger than another and unequal subdiv face sizes cause 
   approximation errors to be magnified.

サブディビジョンサーフェスは滑らかで制御しやすい極限サーフェスを持ったキャラクター形状
の最終レンダリングのために一般的に用いられています。ただし、インタラクティブな
アプリケーション内ではパフォーマンスの事情によりポリゴンでのコントロールメッシュとして
近似的に描画されるのが一般的でした。インタラクティブアプリケーションで近似形状を
操作するため、例えば頬を触る手やボトルをつかむ指などの精密な接触を見るのが難しいという
問題があります。また、肌と衣服がどちらも近似だとクロスシミュレーションの突き抜けをチェック
するのも難しくなります。特に、接触する片方のフェイスのサイズが相手より著しく大きい場合
などでは近似誤差が大きくなり、顕著な問題となって現れます。


.. Maya and Pixar's proprietary Presto animation system can take 100ms to subdivide 
   a character of 30,000 polygons to the second level of subdivision (500,000 polygons). 
   Being able to perform the same operation in less than 3ms allows the user to interact
   with the smooth, accurate limit surface at all times.

Maya、およびピクサーのインハウスツール Presto アニメーションシステムでは 3万ポリゴンの
キャラクターを２レベルサブディビジョンして50万ポリゴンにするのに100ミリ秒かかります。
もしこれを3ミリ秒以下にすることができれば、ユーザが滑らかで正確な極限サーフェスを常に
操作することができるようになります。

.. image:: images/osd_splash.png 
   :align: center
   :target: images/osd_splash.png 



----

.. Research

理論研究
===========


.. The new GPU technology behind OpenSubdiv is the result of a joint research effort
   between Pixar and Microsoft.

OpenSubdiv を支える新しい GPU 技術はピクサーとマイクロソフトの共同研究の成果として得られました。

    | *Feature Adaptive GPU Rendering of Catmull-Clark Subdivision Surfaces*
    | Matthias Niessner, Charles Loop, Mark Meyer, and Tony DeRose
    | ACM Transactions on Graphics, Vol. 31 No. 1 Article 6 January 2012 
    | `<http://research.microsoft.com/en-us/um/people/cloop/tog2012.pdf>`_
    |
    | *Efficient Evaluation of Semi-Smooth Creases in Catmull-Clark Subdivision Surfaces*
    | Matthias Niessner, Charles Loop, and Guenter Greiner.
    | Eurographics Proceedings, Cagliari, 2012
    | `<http://research.microsoft.com/en-us/um/people/cloop/EG2012.pdf>`_
    |
    | *Analytic Displacement Mapping using Hardware Tessellation*
    | Matthias Niessner, Charles Loop
    | ACM Transactions on Graphics, To appear 2013
    | `<http://research.microsoft.com/en-us/um/people/cloop/TOG2013.pdf>`_
    
----

.. Heritage
   
歴史
========

.. This is the fifth-generation subdiv library in use by Pixar's proprietary animation 
   system in a lineage that started with code written by Tony DeRose and Tien Truong 
   for Geri's Game in 1996. Each generation has been a from-scratch rewrite that 
   has built upon our experience using subdivision surfaces to make animated films. 
   This code is live, so Pixar's changes to OpenSubdiv for current and future films 
   will be released as open source at the same time they are rolled out to Pixar 
   animation production.

これはピクサーのインハウスアニメーションシステムでは５世代目のサブディビジョンサーフェス
ライブラリですが、そのオリジナルは 1996 年、ゲーリーじいさんのチェスの制作に際して Tony DeRose,
Tien Truong らによって初めて書かれたものです。ピクサーのアニメーション制作の経験を
活かしながら、世代を重ねる度に毎回完全に書きなおされて来ました。このコードは今現在で
実際に使われているものであり、今および未来の映画のためにピクサーが OpenSubdiv に対して
行う変更は、ピクサーのアニメーション制作に投入されるのと同時にオープンソースとして
リリースしていきます。


    | *Subdivision for Modeling and Animation*
    | Denis Zorin, Peter Schroder
    | Course Notes of SIGGRAPH 1999
    | `<http://www.multires.caltech.edu/pubs/sig99notes.pdf>`_
    |
    | *Subdivision Surfaces in Character Animation*
    | Tony DeRose, Michael Kass, Tien Truong
    | Proceedings of SIGGRAPH 1998
    | `<http://graphics.pixar.com/library/Geri/paper.pdf>`_
    |
    | *Recursively generated B-spline surfaces on arbitrary topological meshes*
    | Catmull, E.; Clark, J. Computer-Aided Design 10 (6) (1978)

----

.. Licensing

ライセンス
=============

.. OpenSubdiv is covered by the Apache License, and is free to use for commercial or
   non-commercial use. This is the same code that Pixar uses internally for animated
   film production. Our intent is to encourage a geometry standard for subdivision 
   surfaces, by providing consistent (i.e. yielding the same limit surface), high 
   performance implementations on a variety of platforms.
   
OpenSubdiv は Apache License で提供されており、商用・非商用ともに無料で使用可能です。
これはピクサーが社内で使っているアニメーション映画制作用のソフトウェアと同一のコードです。
私達のねらいは統一されたアルゴリズムで同一の極限サーフェスを得られ、かつ様々なプラットフォームで
高パフォーマンスが得られるライブラリを提供することにより、サブディビジョンサーフェスを
形状表現のスタンダードとしてすすめていくことです。

.. Why Apache? We were looking for a commercial-friendly license that would convey 
   our patents to the end users. This quickly narrowed the field to Microsoft Public 
   License or Apache. Initially we chose MSPL because it handled trademarks better. 
   But at the request of several companies we gave Apache another look, and decided 
   to go with Apache with a very slight modification that simply says you cannot use 
   any contributors' trademarks. In other words, you can use OpenSubdiv to make a 
   product, but you cannot use a Luxo Lamp (or other character, etc.) when marketing 
   your product.

Apache ライセンスの理由：私達は商用に適用しやすく、かつ関連特許も扱えるライセンスを
選び、Microsoft Public License と Apache ライセンスが候補になりました。当初は知的所有権
の取り扱いが良かった MSPL を採用していましたが、その後様々な要望を頂いた上で Apache
ライセンスを再検討し、商標に関するわずかな変更を加えて Apache ライセンスとすることに
しました。例えば、OpenSubdiv を製品に使うことはできますが、Luxo やピクサーのキャラクターを
その製品のマーケティングのためなどに用いることはできません。

`License Header <license.html>`_

----

.. Contributing
   
開発への参加
============

.. For details on how to contribute to OpenSubdiv, see the page on 
   `Contributing <contributing.html>`_

詳細は `Contributing <contributing.html>`_ のページを御覧ください。

----

External Resources
==================

Microsoft Research:
    `Charles Loop <http://research.microsoft.com/en-us/um/people/cloop/>`__
    `Matthias Niessner <http://lgdv.cs.fau.de/people/card/matthias/niessner/>`__

Pixar Research:
    `Pixar R&D Portal <http://graphics.pixar.com/research/>`__

