#!/bin/bash

SCRIPT_DIR=$(cd $(dirname "$0") && pwd)

function tolower() {
  echo $1 | tr '[:upper:]' '[:lower:]'
}

function locate() {
  echo $(find $MODEL_BASE_FOLDER/ -name $1 2>/dev/null | head)
}

MODEL_BASE_FOLDER="$1"
SERVER_NAME="$2"
SRC_LANG_ID=$(tolower "$3")
TGT_LANG_ID=$(tolower "$4")

SHORTLIST_FILE=$(locate "lex.${SRC_LANG_ID}${TGT_LANG_ID}.bin")

SRC_SPM_FILE=$(locate "${SRC_LANG_ID}.spm")
TGT_SPM_FILE=$(locate "${TGT_LANG_ID}.spm")
CMN_SPM_FILE=$(locate "*.spm")

SRC_BPE_FILE=$(locate "${SRC_LANG_ID}.bpe")
TGT_BPE_FILE=$(locate "${TGT_LANG_ID}.bpe")
CMN_BPE_FILE=$(locate "*.bpe")

SRC_VCB_FILE=$(locate "vocab.${SRC_LANG_ID}.yml")
TGT_VCB_FILE=$(locate "vocab.${TGT_LANG_ID}.yml")
CMB_VCB_FILE=$(locate "vocab.yml")

MODEL_FILE=$(find $MODEL_BASE_FOLDER -name "model*.bin" 2>/dev/null)
INT_SHIFT_ALPHA_PARAM=$(egrep ".*alphas.bin" <<< "$MODEL_FILE" >/dev/null 2>& 1 && echo "--int8shiftAlpha")

if [ -n "$SRC_SPM_FILE" -a -n "$TGT_SPM_FILE" ]; then
  VOCAB_PARAMS="-v $SRC_SPM_FILE $TGT_SPM_FILE"
elif [ -n "$CMN_SPM_FILE" ]; then
  VOCAB_PARAMS="-v $CMN_SPM_FILE $CMN_SPM_FILE"
elif [ -n "$SRC_VCB_FILE" -a -n "$TGT_VCB_FILE" ]; then
  VOCAB_PARAMS="-v $SRC_VCB_FILE $TGT_VCB_FILE"
elif [ -n "$CMB_VCB_FILE" ]; then
  VOCAB_PARAMS="-v $CMB_VCB_FILE $CMB_VCB_FILE"
else
  echo "Error locating vocabulary files." 1>&2
  exit 1
fi

BPE_PARAMS=""
if [ -n "$SRC_BPE_FILE" -a -n "$TGT_BPE_FILE" ]; then
  BPE_PARAMS="--bpe_file $SRC_BPE_FILE $TGT_BPE_FILE"
elif [ -n "$CMN_BPE_FILE" ]; then
  BPE_PARAMS="--bpe_file $CMN_BPE_FILE"
fi

echo $SCRIPT_DIR/TargomanNMTServer --server_name $SERVER_NAME --shortlist $SHORTLIST_FILE -m $MODEL_FILE $BPE_PARAMS $VOCAB_PARAMS $INT_SHIFT_ALPHA_PARAM
$SCRIPT_DIR/TargomanNMTServer --server_name $SERVER_NAME --shortlist $SHORTLIST_FILE -m $MODEL_FILE $BPE_PARAMS $VOCAB_PARAMS $INT_SHIFT_ALPHA_PARAM
