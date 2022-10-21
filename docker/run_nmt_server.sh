#!/bin/bash

SCRIPT_DIR=$(cd $(dirname "$0") && pwd)

function tolower() {
  echo $1 | tr '[:upper:]' '[:lower:]'
}

function locate_file() {
  local FILE=$(find $MODEL_BASE_FOLDER/ -name $1 2>/dev/null)
  echo $FILE
}

function locate_vocabs() {
}

SERVER_NAME="$1"
SRC_LANG_ID="$2"
TRG_LANG_ID="$3"

MODEL_BASE_FOLDER=$(find . -name ${SRC_LANG_ID}2${TRG_LANG_ID} 2>/dev/null)
if [ -z $MODEL_BASE_FOLDER ]; then
  echo "Error locating the translation model." 1>&2
  exit 1
fi

SHORTLIST_FILE=$(find $MODEL_BASE_FOLDER -name "lex*.bin" 2>/dev/null)
MODEL_FILE=$(find $MODEL_BASE_FOLDER -name "model*.bin" 2>/dev/null)
FIRST_SPM_FILE=$(find $MODEL_BASE_FOLDER -name "*.spm" 2>/dev/null | head)
FIRST_BPE_FILE=$(find $MODEL_BASE_FOLDER -name "*.bpe" 2>/dev/null | head)
if [ -r $MODEL_BASE_FOLDER/$(tolower ${SRC_LANG_ID}).spm ] && [ -r $MODEL_BASE_FOLDER/$(tolower ${TRG_LANG_ID}).spm ]; then
  VOCAB_PARAMS="-v $MODEL_BASE_FOLDER/$(tolower ${SRC_LANG_ID}).spm $MODEL_BASE_FOLDER/$(tolower ${TRG_LANG_ID}).spm"
elif [ -r $MODEL_BASE_FOLDER/$(tolower ${SRC_LANG_ID}).bpe ] && [ -r $MODEL_BASE_FOLDER/$(tolower ${TRG_LANG_ID}).bpe ]; then
  VOCAB_PARAMS="--bpe_file $MODEL_BASE_FOLDER/$(tolower ${SRC_LANG_ID}).bpe $MODEL_BASE_FOLDER/$(tolower ${TRG_LANG_ID}).bpe -v $MODEL_BASE_FOLDER/vocab.$(tolower ${SRC_LANG_ID}).yml $MODEL_BASE_FOLDER/vocab.$(tolower ${TRG_LANG_ID}).yml"
elif [ ! -z "$FIRST_SPM_FILE" ]; then
  VOCAB_PARAMS="-v $FIRST_SPM_FILE $FIRST_SPM_FILE"
elif [ ! -z "$FIRST_BPE_FILE" ]; then
  VOCAB_PARAMS="--bpe_file $FIRST_BPE_FILE -v $MODEL_BASE_FOLDER/vocab.$(tolower ${SRC_LANG_ID}).yml $MODEL_BASE_FOLDER/vocab.$(tolower ${TRG_LANG_ID}).yml"
else
  VOCAB_PARAMS="-v $MODEL_BASE_FOLDER/vocab.$(tolower ${SRC_LANG_ID}).yml $MODEL_BASE_FOLDER/vocab.$(tolower ${TRG_LANG_ID}).yml"
fi
INT_SHIFT_ALPHA_PARAM=$(egrep ".*alphas.bin" <<< "$MODEL_FILE" >/dev/null 2>& 1 && echo "--int8shiftAlpha")

echo SHORTLIST_FILE=$SHORTLIST_FILE
echo MODEL_FILE=$MODEL_FILE
echo VOCAB_PARAMS=$VOCAB_PARAMS
echo INT_SHIFT_ALPHA_PARAM=$INT_SHIFT_ALPHA_PARAM
echo $SCRIPT_DIR/TargomanNMTServer --server_name $SERVER_NAME --shortlist $SHORTLIST_FILE -m $MODEL_FILE $VOCAB_PARAMS $INT_SHIFT_ALPHA
$SCRIPT_DIR/TargomanNMTServer --server_name $SERVER_NAME --shortlist $SHORTLIST_FILE -m $MODEL_FILE $VOCAB_PARAMS $INT_SHIFT_ALPHA

