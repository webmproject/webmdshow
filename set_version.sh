#!/bin/bash
##
##  Copyright (c) 2014 The WebM project authors. All Rights Reserved.
##
##  Use of this source code is governed by a BSD-style license
##  that can be found in the LICENSE file in the root of the source
##  tree. An additional intellectual property rights grant can be found
##  in the file PATENTS.  All contributing project authors may
##  be found in the AUTHORS file in the root of the source tree.
##
. $(dirname $0)/../common/common.sh

readonly dshow_rcfiles="makewebm
                        playwebm
                        vp8decoder
                        vp8encoder
                        vp9decoder
                        vpxdecoder
                        webmmux
                        webmsource
                        webmsplit
                        webmcc
                        webmvorbisencoder
                        webmvorbisdecoder
                        webmoggsource"
readonly mf_rcfiles="webmmfsource
                     webmmfvorbisdec
                     webmmfvp8dec"




