#include <algorithm>
#include <future>
#include <map>
#include <memory>
#include <regex>
#include "bpe.h"
#include "json.hpp"
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wsign-compare"
#pragma GCC diagnostic ignored "-Wunused-parameter"
#pragma GCC diagnostic ignored "-Wunused-value"
#include <marian/common/project_version.h>
#include <marian/common/cli_wrapper.h>
#include <marian/common/config_parser.h>
#include <marian/common/logging.h>
#include <marian/common/utils.h>
#include <marian/data/batch_generator.h>
#include <marian/data/corpus.h>
#include <marian/data/dataset.h>
#include <marian/data/shortlist.h>
#include <marian/data/text_input.h>
#include <marian/data/types.h>
#include <marian/marian.h>
#include <marian/translator/beam_search.h>
#include <marian/translator/output_collector.h>
#pragma GCC diagnostic pop
#include "server.h"
#include "./debug.h"

using namespace marian;

#if (PROJECT_VERSION_MAJOR < 1) || (PROJECT_VERSION_MINOR < 10)
namespace marian {
template<typename T>
using IPtr=marian::Ptr<T>;
}
#endif

typedef std::pair<std::vector<std::string>, std::vector<int>> InputLines_t;
const std::regex gLineEnderRegEx(" [\\.\\?\\!] ");
InputLines_t getTextLines(const clsSimpleRestRequest& _request) {
  InputLines_t Result;

  std::string Body = _request.getBody();
  LOG(info, "[rest-server] Request with BODY\n{}\n", Body);

  auto JSON = nlohmann::json::parse(Body);

  if(!JSON.is_array())
    throw std::runtime_error("Input string must be a JSON array.");
  
  int LineIndex = 0;
  for(size_t k = 0; k < JSON.size(); ++k) {
    auto& Element = JSON.at(k);
    if(!Element.is_object() || !Element.contains("src"))
      throw std::runtime_error("Each element of the input array must be an object containing `src` field.");
    if(!Element["src"].is_string())
      throw std::runtime_error("`src` field must contain string.");
    std::string Src = Element["src"].get<std::string>();
    std::smatch LineEnderMatch;
    int i = 0;
    while(std::regex_search(Src, LineEnderMatch, gLineEnderRegEx)) {
      int j = LineEnderMatch.position() + 2;
      if(j > i) {
        Result.first.push_back(Src.substr(i, j - i));
        Result.second.push_back(LineIndex);
      }
      Src = LineEnderMatch.suffix();
    }
    if(Src.size()) {
      Result.first.push_back(Src);
      Result.second.push_back(LineIndex);
    }
    ++LineIndex;
  }
  return Result;
}

typedef std::vector<std::vector<float>> Matrix_t;
typedef std::pair<std::vector<std::string>, Matrix_t> TranslationResult_t;
typedef std::pair<std::vector<std::string>, std::vector<TranslationResult_t>> TranslateFuncResult_t;

class TextTokensInput;

class TextTokensIterator : public IteratorFacade<TextTokensIterator, data::SentenceTuple const> {
public:
  TextTokensIterator() : pos_(-1), tup_(0) {}
  explicit TextTokensIterator(TextTokensInput& corpus);

private:
  void increment() override;

  bool equal(TextTokensIterator const& other) const override {
    return this->pos_ == other.pos_ || (this->tup_.empty() && other.tup_.empty());
  }

  const data::SentenceTuple& dereference() const override { return tup_; }

  TextTokensInput* corpus_;

  long long int pos_;
  data::SentenceTuple tup_;
};

class TextTokensInput
    : public data::DatasetBase<data::SentenceTuple, TextTokensIterator, data::CorpusBatch> {
private:
  std::vector<std::vector<Word>> lines_;
  std::vector<Ptr<Vocab>> vocabs_;

  size_t pos_{0};

public:
  typedef data::SentenceTuple Sample;

  TextTokensInput(std::vector<std::vector<Word>> inputs,
                  std::vector<Ptr<Vocab>> vocabs,
                  Ptr<Options> options)
      : DatasetBase({"./dummy.stream"}, options), lines_(inputs), vocabs_(vocabs) {}

  Sample next() override {
    if(pos_ < lines_.size()) {
      size_t curId = pos_++;
      // fill up the sentence tuple with sentences from all input files
      data::SentenceTuple tup(curId);
      Words words = lines_[curId];
      if(words.empty())
#if (PROJECT_VERSION_MAJOR < 1) || (PROJECT_VERSION_MINOR < 10)
        words.push_back(0);
#else
        words.push_back(Word::fromWordIndex(0));
#endif
      tup.push_back(words);
      return tup;
    }
    return data::SentenceTuple(0);
  }

  void shuffle() override {}
  void reset() override {}

  iterator begin() override { return iterator(*this); }
  iterator end() override { return iterator(); }

  // TODO: There are half dozen functions called toBatch(), which are very
  // similar. Factor them.
  batch_ptr toBatch(const std::vector<Sample>& batchVector) override {
    size_t batchSize = batchVector.size();

    std::vector<size_t> sentenceIds;

    std::vector<int> maxDims;
    for(auto& ex : batchVector) {
      if(maxDims.size() < ex.size())
        maxDims.resize(ex.size(), 0);
      for(size_t i = 0; i < ex.size(); ++i) {
        if(ex[i].size() > (size_t)maxDims[i])
          maxDims[i] = (int)ex[i].size();
      }
      sentenceIds.push_back(ex.getId());
    }

    std::vector<Ptr<data::SubBatch>> subBatches;
    for(size_t j = 0; j < maxDims.size(); ++j) {
      subBatches.emplace_back(New<data::SubBatch>(batchSize, maxDims[j], vocabs_[j]));
    }

    std::vector<size_t> words(maxDims.size(), 0);
    for(size_t i = 0; i < batchSize; ++i) {
      for(size_t j = 0; j < maxDims.size(); ++j) {
        for(size_t k = 0; k < batchVector[i][j].size(); ++k) {
          subBatches[j]->data()[k * batchSize + i] = batchVector[i][j][k];
          subBatches[j]->mask()[k * batchSize + i] = 1.f;
          words[j]++;
        }
      }
    }

    for(size_t j = 0; j < maxDims.size(); ++j)
      subBatches[j]->setWords(words[j]);

    auto batch = batch_ptr(new batch_type(subBatches));
    batch->setSentenceIds(sentenceIds);

    return batch;
  }

  void prepare() override {}
};

TextTokensIterator::TextTokensIterator(TextTokensInput& corpus)
    : corpus_(&corpus), pos_(0), tup_(corpus_->next()) {}
void TextTokensIterator::increment() {
  tup_ = corpus_->next();
  pos_++;
}

class clsOutputCollector {
public:
  clsOutputCollector() : MaxId(-1){};
  clsOutputCollector(const clsOutputCollector&) = delete;

  void add(long _sourceId,
           const std::vector<std::string>& _sentence,
           const std::vector<TranslationResult_t>& _translation) {
    std::lock_guard<std::mutex> Lock(this->Mutex);
    this->Outputs[_sourceId] = std::make_pair(_sentence, _translation);
    if(this->MaxId <= _sourceId)
      this->MaxId = _sourceId;
  }
  std::vector<TranslateFuncResult_t> collect() {
    std::vector<TranslateFuncResult_t> Result;
    for(int Id = 0; Id <= this->MaxId; ++Id)
      Result.emplace_back(this->Outputs[Id]);
    return Result;
  }

protected:
  long MaxId;
  std::mutex Mutex;

  typedef std::map<long, TranslateFuncResult_t> Outputs_t;
  Outputs_t Outputs;
};

class clsNBestCollector {
public:
  clsNBestCollector() : MaxId(-1){};
  clsNBestCollector(const clsNBestCollector&) = delete;

  void add(long _sourceId, std::vector<std::string>&& _nBest) {
    std::lock_guard<std::mutex> Lock(this->Mutex);
    this->Outputs[_sourceId] = std::move(_nBest);
    if(this->MaxId <= _sourceId)
      this->MaxId = _sourceId;
  }
  std::vector<std::vector<std::string>> collect() {
    std::vector<std::vector<std::string>> Result;
    for(int Id = 0; Id <= this->MaxId; ++Id) {
      Result.emplace_back(std::move(this->Outputs[Id]));
    }
    return Result;
  }

protected:
  long MaxId;
  std::mutex Mutex;

  typedef std::map<long, std::vector<std::string>> Outputs_t;
  Outputs_t Outputs;
};

template <class Search>
class TranslateService {
private:
  Ptr<Options> options_;
  std::vector<Ptr<ExpressionGraph>> graphs_;
  std::vector<std::vector<Ptr<Scorer>>> scorers_;

  std::vector<Ptr<Vocab>> srcVocabs_;
  Ptr<Vocab> trgVocab_;

  size_t numDevices_;
  Ptr<BPE> bpe_;

  std::vector<DeviceId> devices;

  Ptr<ThreadPool> threadPool_;
  std::atomic_long batchId;  

public:
  virtual ~TranslateService() {}

  TranslateService(Ptr<Options> options) : options_(options) { init(); }

  std::vector<TranslationResult_t> convertToTranslationResultVector(
      const Ptr<History>& _history,
      const std::vector<int>& _wordIndexes,
      size_t _wordCount) {
#if (PROJECT_VERSION_MAJOR < 1) || (PROJECT_VERSION_MINOR < 10)
    auto NBest = _history->NBest(SIZE_MAX);
#else
    auto NBest = _history->nBest(SIZE_MAX);
#endif
    std::vector<TranslationResult_t> Result;
    for(const auto& Item : NBest) {
      Words OutputWordVocabIds;
      IPtr<Hypothesis> Hypothesis;
      float Score;
      std::tie(OutputWordVocabIds, Hypothesis, Score) = Item;
      std::vector<std::string> OutputWordsBpe(OutputWordVocabIds.size() - 1);
      EXTRA_DEBUG(std::cout << "Getting BPE words ..." << std::endl;)
      for(size_t i = 0; i < OutputWordVocabIds.size() - 1; ++i) {
        OutputWordsBpe[i] = this->trgVocab_->operator[](OutputWordVocabIds[i]);
      }
      std::vector<int> OutputWordIndexes;
      std::vector<std::string> OutputWords;
      EXTRA_DEBUG(std::cout << "Decoding BPE ..." << std::endl;)
      std::tie(OutputWordIndexes, OutputWords) = this->bpe_->Decode(OutputWordsBpe);
      EXTRA_DEBUG(std::cout << "Getting soft alignment ...  " << _wordCount << ":" << OutputWordIndexes.size() << std::endl;)
#if (PROJECT_VERSION_MAJOR < 1) || (PROJECT_VERSION_MINOR < 10)
      data::SoftAlignment RawAlignment = Hypothesis->TracebackAlignment();
#else
      data::SoftAlignment RawAlignment = Hypothesis->tracebackAlignment();
#endif
      EXTRA_DEBUG(std::cout << "Converting soft alignment ... " << RawAlignment.size() << "x" << RawAlignment[0].size() <<  std::endl;)
      data::SoftAlignment Alignment(OutputWords.size());
      for(size_t i = 0; i < OutputWords.size(); ++i) {
        Alignment[i].resize(_wordCount);
        for(size_t j = 0; j < _wordCount; ++j)
          Alignment[i][j] = 0;
      }
      EXTRA_DEBUG(std::cout << "Alignment: " << Alignment.size() << "x" << Alignment[0].size() << std::endl;)
      for(size_t i = 0; i < OutputWordIndexes.size(); ++i) {
        auto k = OutputWordIndexes[i];
        for(size_t j = 0; j < _wordIndexes.size(); ++j) {
          auto l = _wordIndexes[j];
	        EXTRA_DEBUG(std::cout << "k,l <= i, j ... " << k << "," << l << " <= " << i << "," << j << std::endl;)
          Alignment[k][l] += RawAlignment[i][j];
        }
      }
      EXTRA_DEBUG(std::cout << "Emplacing result item ..." << std::endl;)

      Result.emplace_back(std::make_pair(OutputWords, Alignment));
    }
    EXTRA_DEBUG(std::cout << "Returining all results!" << std::endl;)
    return Result;
  }

  std::vector<std::string> tokenizeSource(const std::string _sentence) {
    auto WordIndexes = this->srcVocabs_[0]->encode(_sentence, false, true);
    std::vector<std::string> Words(WordIndexes.size());
    std::transform(WordIndexes.begin(), WordIndexes.end(), Words.begin(), [&](const IndexType& e) {
#if (PROJECT_VERSION_MAJOR < 1) || (PROJECT_VERSION_MINOR < 10)
      return this->srcVocabs_[0]->operator[](e);
#else
      return this->srcVocabs_[0]->operator[](Word::fromWordIndex(e));
#endif
    });
    return Words;
  }

  void init() {
    // initialize vocabs

    this->bpe_ = nullptr;
    if(options_->get<std::string>("bpe_file").size() > 0)
      this->bpe_ = New<BPE>(options_->get<std::string>("bpe_file"), "@@");

    auto vocabPaths = options_->get<std::vector<std::string>>("vocabs");
    std::vector<int> maxVocabs = options_->get<std::vector<int>>("dim-vocabs");

    for(size_t i = 0; i < vocabPaths.size() - 1; ++i) {
      Ptr<Vocab> vocab = New<Vocab>(options_, i);
      vocab->load(vocabPaths[i], maxVocabs[i]);
      srcVocabs_.emplace_back(vocab);
    }

    trgVocab_ = New<Vocab>(options_, vocabPaths.size() - 1);
    trgVocab_->load(vocabPaths.back());

    // get device IDs
    devices = Config::getDevices(options_);
    numDevices_ = devices.size();

    this->threadPool_ = New<ThreadPool>(numDevices_, numDevices_);
    this->batchId = 0;

    // initialize scorers
    for(auto device : devices) {
      auto graph = New<ExpressionGraph>(true);
      graph->setDevice(device);
#if (PROJECT_VERSION_MAJOR < 1) || (PROJECT_VERSION_MINOR < 10)
      graph->getBackend()->setClip(options_->get<float>("clip-gemm"));
#endif
      graph->reserveWorkspaceMB(options_->get<size_t>("workspace"));
      graphs_.push_back(graph);

      auto scorers = createScorers(options_);
      for(auto scorer : scorers)
        scorer->init(graph);
      scorers_.push_back(scorers);
    }
  }

  template<typename CollectionCallback>
  void runOnCorpusAndCollect(
    data::BatchGenerator<TextTokensInput>& batchGenerator,
    CollectionCallback collect
  ) {
    batchGenerator.prepare();

    std::vector<std::future<void>> taskResults;
    for(auto batch : batchGenerator) {
      auto task = [=](size_t id) {
        thread_local Ptr<ExpressionGraph> graph;
        thread_local std::vector<Ptr<Scorer>> scorers;

        if(!graph) {
          graph = graphs_[id % numDevices_];
          scorers = scorers_[id % numDevices_];
        }

        EXTRA_DEBUG(std::cout << "Creating search ..." << std::endl;)
#if (PROJECT_VERSION_MAJOR < 1) || (PROJECT_VERSION_MINOR < 10)
        auto search = New<Search>(options_, scorers, trgVocab_->getEosId(), trgVocab_->getUnkId());
#else
        auto search = New<Search>(options_, scorers, trgVocab_);
#endif
        EXTRA_DEBUG(std::cout << "Searching ..." << std::endl;)
        auto histories = search->search(graph, batch);
        EXTRA_DEBUG(std::cout << "Search done." << std::endl;)

        for(auto history : histories) {
#if (PROJECT_VERSION_MAJOR < 1) || (PROJECT_VERSION_MINOR < 10)
          long id = (long)history->GetLineNum();
#else
          long id = (long)history->getLineNum();
#endif
          collect(id, history);
        }
        EXTRA_DEBUG(std::cout << "Task (" << id << ") done." << std::endl;)
      };

      taskResults.emplace_back(threadPool_->enqueue(task, (size_t)batchId));
      batchId++;
    }
    for(auto& f : taskResults)
        f.wait();
  }

/*
  std::vector<std::string>
*/
  std::vector<std::vector<std::string>>
  runFlatForm(const std::vector<std::string> inputs) {
    std::vector<Words> Words;
    for(const auto& line : inputs) {
      Words.emplace_back(this->srcVocabs_[0]->encode(line, true, true));
    }
    auto corpus_ = New<TextTokensInput>(Words, srcVocabs_, this->options_);
    data::BatchGenerator<TextTokensInput> batchGenerator(corpus_, this->options_);

/*
    auto collector = New<StringCollector>();
*/
    auto collector = New<clsNBestCollector>();
    this->runOnCorpusAndCollect(batchGenerator, [&] (long id, Ptr<History>& history) {
      EXTRA_DEBUG(std::cout << "Gathering history (" << id << ")" << std::endl;)
/*
#if (PROJECT_VERSION_MAJOR < 1) || (PROJECT_VERSION_MINOR < 10)
      auto words = std::get<0>(history->Top());
#else
      auto words = std::get<0>(history->top());
#endif
      collector->add(id, this->trgVocab_->decode(words), std::string());
*/
      std::vector<std::string> nBest;
#if (PROJECT_VERSION_MAJOR < 1) || (PROJECT_VERSION_MINOR < 10)
      for(const auto& item : history->NBest(SIZE_MAX)) {
        auto words = std::get<0>(item);
        nBest.push_back(this->trgVocab_->decode(words));
      }
#else
      for(const auto& item : history->nBest(SIZE_MAX)) {
        auto words = std::get<0>(item);
        nBest.push_back(this->trgVocab_->decode(words));
      }
#endif
      collector->add(id, std::move(nBest));
      EXTRA_DEBUG(std::cout << "Gathering history (" << id << ") done." << std::endl;)
    });
/*
    return collector->collect(false);
*/
    return collector->collect();
  }

  std::vector<TranslateFuncResult_t> run(const std::vector<std::string> inputs) {
    std::vector<std::vector<std::string>> WordStrs;
    std::vector<std::vector<Word>> Words;
    std::vector<std::vector<int>> WordIndexes;

    EXTRA_DEBUG(std::cout << "Applying BPE ..." << std::endl;)
    for(const auto& Line : inputs) {
      std::vector<std::string> LineTokens;
      utils::split(Line, LineTokens, " ");
      std::vector<Word> LineWords;
      std::vector<int> LineWordIndexes;
      for(size_t TokenIndex = 0; TokenIndex < LineTokens.size(); ++TokenIndex) {
        const auto& Token = LineTokens[TokenIndex];
        auto Parts = this->bpe_->Encode(Token);
        for(const auto& Part : Parts) {
          auto Word_ = this->srcVocabs_[0]->operator[](Part);
          LineWords.push_back(Word_);
          LineWordIndexes.push_back(TokenIndex);
        }
      }
      LineWords.push_back(this->srcVocabs_[0]->getEosId());
      WordStrs.emplace_back(LineTokens);
      Words.emplace_back(LineWords);
      WordIndexes.emplace_back(LineWordIndexes);
    }
    EXTRA_DEBUG(std::cout << "Applying BPE Done." << std::endl;)

    auto corpus_ = New<TextTokensInput>(Words, srcVocabs_, this->options_);
    data::BatchGenerator<TextTokensInput> batchGenerator(corpus_, this->options_);

    auto collector = New<clsOutputCollector>();
    this->runOnCorpusAndCollect(batchGenerator, [&] (long id, Ptr<History>& history) {
      EXTRA_DEBUG(std::cout << "Gathering history (" << id << ")" << std::endl;)
      collector->add(id, WordStrs.at(id),
        this->convertToTranslationResultVector(
          history,
          WordIndexes.at(id),
          WordStrs.at(id).size()
        )
      );
      EXTRA_DEBUG(std::cout << "Gathering history (" << id << ") done." << std::endl;)
    });
    return collector->collect();
  }
};

typedef std::tuple<std::vector<std::string>,
                   std::vector<std::vector<std::string>>,
                   std::vector<std::vector<int>>>
    OutputItem_t;

const std::vector<std::string> STOPWORDS = {"به",
                                            "در",
                                            "از",
                                            "با",
                                            "تا",
                                            "که",
                                            "را"
                                            "to",
                                            "in",
                                            "from",
                                            "with",
                                            "the"};

std::vector<int> getWordMapping(const Matrix_t& _softAlignmentMatrix,
                                const std::vector<std::string>& _sourceTokens,
                                const std::vector<std::string>& _targetTokens) {
  size_t n = _softAlignmentMatrix.size();     // Target length
  size_t m = _softAlignmentMatrix[0].size();  // Source length

  Matrix_t M = _softAlignmentMatrix;

  for(size_t i = 0; i < n; ++i) {
    if(std::find(STOPWORDS.begin(), STOPWORDS.end(), _targetTokens[i]) != STOPWORDS.end()) {
      for(size_t j = 0; j < m; ++j)
        if(M[i][j] > 0.15) {
          M[i][j] = 0;
        }
    }
  }
  for(size_t j = 0; j < m; ++j) {
    if(std::find(STOPWORDS.begin(), STOPWORDS.end(), _sourceTokens[j]) != STOPWORDS.end()) {
      for(size_t i = 0; i < n; ++i)
        if(M[i][j] > 0.15) {
          M[i][j] = 0;
        }
    }
  }
  std::vector<int> Result(n);
  for(size_t i = 0; i < n; ++i) {
    int MaxIndex = 0;
    float Max = M[i][0];
    for(size_t j = 1; j < m; ++j)
      if(Max < M[i][j]) {
        Max = M[i][j];
        MaxIndex = j;
      }
    Result[i] = MaxIndex;
  }
  return Result;
}

OutputItem_t convertTranslationToOutputItem(
    const std::vector<std::string>& _sourceTokens,
    const std::vector<TranslationResult_t>& _translationResult) {
  const std::vector<std::string>& BestTranslation = _translationResult[0].first;
  const std::vector<int> WordMapping
      = getWordMapping(_translationResult[0].second, _sourceTokens, BestTranslation);

  // std::cout << "****************************************************************" << std::endl;
  // for(const auto& e : WordMapping)
  //   std::cout << e << ",";
  // std::cout << std::endl;
  // std::cout << "****************************************************************" << std::endl;

  std::vector<std::vector<std::string>> Phrases;
  std::vector<std::vector<int>> Alignments;

  size_t PhraseCount = 0;
  size_t LastSourceIndex = (size_t)-1;
  for(size_t TargetIndex = 0; TargetIndex < WordMapping.size(); ++TargetIndex) {
    std::string TargetWord = BestTranslation[TargetIndex];
    size_t SourceIndex = WordMapping[TargetIndex];
    if(TargetWord == DEFAULT_UNK_STR) {
      TargetWord = _sourceTokens[SourceIndex];
    }
    if(SourceIndex == LastSourceIndex) {
      Phrases[PhraseCount - 1][0] += std::string(" ") + TargetWord;
    } else {
      Phrases.push_back({TargetWord});
      Alignments.push_back({static_cast<int>(SourceIndex)});
      ++PhraseCount;
    }
    LastSourceIndex = SourceIndex;
  }

  for(size_t NBestIndex = 1; NBestIndex < _translationResult.size(); ++NBestIndex) {
    const std::vector<std::string>& TranslationCandidate = _translationResult[NBestIndex].first;
    std::vector<int> WordMapping = getWordMapping(
        _translationResult[NBestIndex].second, _sourceTokens, TranslationCandidate);

    size_t LastSourceIndex = (size_t)-1;
    std::vector<std::string> CandidatesBySource;
    CandidatesBySource.resize(_sourceTokens.size());
    for(size_t TargetIndex = 0; TargetIndex < WordMapping.size(); ++TargetIndex) {
      auto TargetWord = TranslationCandidate[TargetIndex];
      size_t SourceIndex = WordMapping[TargetIndex];
      if(TargetWord == DEFAULT_UNK_STR) {
        TargetWord = _sourceTokens[SourceIndex];
      }
      if(CandidatesBySource[SourceIndex].size() > 0) {
        if(SourceIndex == LastSourceIndex)
          CandidatesBySource[SourceIndex] += std::string(" ") + TargetWord;
      } else {
        CandidatesBySource[SourceIndex] = TargetWord;
      }
      LastSourceIndex = SourceIndex;
    }

    for(size_t PhraseIndex = 0; PhraseIndex < Phrases.size(); ++PhraseIndex) {
      size_t SourceIndex = Alignments[PhraseIndex][0];
      std::vector<std::string>& Candidates = Phrases[PhraseIndex];
      bool Found = false;
      for(size_t CandidateIndex = 0; CandidateIndex < Candidates.size(); ++CandidateIndex)
        if(Candidates[CandidateIndex] == CandidatesBySource[SourceIndex]) {
          Found = true;
          break;
        }
      if(Found == false) {
        Candidates.push_back(CandidatesBySource[SourceIndex]);
      }
    }
  }

  return std::make_tuple(_sourceTokens, Phrases, Alignments);
}

void mergeToPreviousOutputItem(OutputItem_t& _item, const OutputItem_t& _secondItem) {
  std::vector<std::string>& SourceTokens = std::get<0>(_item);
  std::vector<std::vector<std::string>>& Phrases = std::get<1>(_item);
  std::vector<std::vector<int>>& Alignments = std::get<2>(_item);

  std::vector<std::string> ExtSourceTokens;
  std::vector<std::vector<std::string>> ExtPhrases;
  std::vector<std::vector<int>> ExtAlignments;

  std::tie(ExtSourceTokens, ExtPhrases, ExtAlignments) = _secondItem;

  int SourceOffset = SourceTokens.size();
  // int TargetOffset = Phrases.size();
  for(const auto& Word : ExtSourceTokens)
    SourceTokens.push_back(Word);
  for(const auto& Phrase : ExtPhrases)
    Phrases.push_back(Phrase);
  for(const auto& A : ExtAlignments) {
    std::vector<int> UpdatedAlignments;
    for(auto a : A)
      UpdatedAlignments.push_back(a + SourceOffset);
    Alignments.push_back(UpdatedAlignments);
  }
  // std::cout << "/////////////////////////////////////////////////////" << std::endl;
  // for(size_t ii = 0; ii < Alignments.size(); ++ii) {
  //   for(size_t jj = 0; jj < Alignments[ii].size(); ++jj) {
  //     std::cout << Alignments[ii][jj] << ", ";
  //   }
  //   std::cout << std::endl;
  // }
  // std::cout << "/////////////////////////////////////////////////////" << std::endl;
}

int main(int argc, char* argv[]) {

#if (PROJECT_VERSION_MAJOR > 1) || (PROJECT_VERSION_MINOR > 7)
  ConfigParser Parser(cli::mode::translation);
  Parser.addOption<std::string>(
    "--bpe_file",
    "General options",
    "Path to bpe model for use with tokenization of input string",
    "");
  Parser.addOption<bool>(
    "--extra_debug",
    "General options",
    "Enable/Disable extra debugging logs",
    false);
  Parser.addOption<std::string>(
    "--server_name",
    "General options",
    "Name to identify model/engine combination",
    "");
  CommandLineOptions = Parser.parseOptions(argc, argv, true);
#else
  if(true) {
    std::vector<char*> filtered_args;
    std::string bpeFile = "", serverName = "";
    bool extraDebug = false;
    int argIndex = 0;
    while(argIndex < argc) {
      if(strcmp(argv[argIndex], "--bpe_file") == 0) {
        ++argIndex;
        bpeFile = argIndex < argc ? argv[argIndex] : "";
      } else if(strcmp(argv[argIndex], "--server_name") == 0) {
        ++argIndex;
        serverName = argIndex < argc ? argv[argIndex] : "";
      } else if(strcmp(argv[argIndex], "--extra_debug") == 0) {
        extraDebug = true;
      } else
        filtered_args.push_back(argv[argIndex]);
      ++argIndex;
    }
    CommandLineOptions = parseOptions(
      static_cast<int>(filtered_args.size()),
      const_cast<char**>(filtered_args.data()),
      cli::mode::translation,
      true);
    CommandLineOptions->set("extra_debug", extraDebug);
    CommandLineOptions->set("bpe_file", bpeFile);
    CommandLineOptions->set("server_name", serverName);
  }
#endif

  auto& Options = CommandLineOptions;
  Options->set("inference", true);
  Options->set("n-best", true);
  Options->set("alignment", "soft");
  Options->set("allow-unk", true);

  const std::string ServerName(Options->get<std::string>("server_name", ""));

  TranslateService<BeamSearch> TranslationService(Options);

  clsSimpleRestServer Server("0.0.0.0", 8080, 16);
  if(Options->get<std::string>("bpe_file").size() > 0) {
    Server.setCallback([&](const clsSimpleRestRequest& _request, clsSimpleRestResponse& _response) {
      std::vector<std::string> Sentences;
      std::vector<int> LineNumbers;
      std::tie(Sentences, LineNumbers) = getTextLines(_request);
      auto RawResults = TranslationService.run(Sentences);

      std::vector<OutputItem_t> Result;
      int PreviousLineNumber = -1;
      for(size_t Dummy = 0; Dummy < RawResults.size(); ++Dummy) {
        std::vector<std::string> SourceTokens;
        std::vector<TranslationResult_t> NBestTranslations;
        std::tie(SourceTokens, NBestTranslations) = RawResults[Dummy];
        if(NBestTranslations.size() > 0) {
          OutputItem_t Item = convertTranslationToOutputItem(SourceTokens, NBestTranslations);
          if(PreviousLineNumber == LineNumbers[Dummy]) {
            mergeToPreviousOutputItem(Result[Result.size() - 1], Item);
            // const auto& Alignments = std::get<2>(Result[Result.size() - 1]);
            // std::cout << "\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\" << std::endl;
            // for(size_t ii = 0; ii < Alignments.size(); ++ii) {
            //   for(size_t jj = 0; jj < Alignments[ii].size(); ++jj) {
            //     std::cout << Alignments[ii][jj] << ", ";
            //   }
            //   std::cout << std::endl;
            // }
            // std::cout << "\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\" << std::endl;
          } else
            Result.push_back(Item);
        } else {
          Result.push_back(std::make_tuple(std::vector<std::string>(),
                                          std::vector<std::vector<std::string>>(),
                                          std::vector<std::vector<int>>()));
        }
        PreviousLineNumber = LineNumbers[Dummy];
      }

      nlohmann::json ResponseJSON;
      auto& Rslt = ResponseJSON["rslt"] = nlohmann::json::array();
      for(const auto& Item : Result) {
        std::vector<std::string> SourceTokens;
        std::vector<std::vector<std::string>> Phrases;
        std::vector<std::vector<int>> Alignments;
        std::tie(SourceTokens, Phrases, Alignments) = Item;
        nlohmann::json RsltItem;
        RsltItem["tokens"] = SourceTokens;
        RsltItem["phrases"] = Phrases;
        RsltItem["alignments"] = Alignments;
        Rslt.push_back(RsltItem);
      }
      ResponseJSON["serverName"] = ServerName;
      auto ResponseJsonString = ResponseJSON.dump();
      _response << ResponseJsonString.c_str();
      _response.send();
      LOG(info, "[rest server] Request answered.");
    });
  } else {
    Server.setCallback([&](const clsSimpleRestRequest& _request, clsSimpleRestResponse& _response) {
      std::vector<std::string> Sentences;
      std::vector<int> LineNumbers;
      std::tie(Sentences, LineNumbers) = getTextLines(_request);
      auto Results = TranslationService.runFlatForm(Sentences);
      if(Sentences.size() != Results.size() || Sentences.size() != LineNumbers.size())
        throw std::runtime_error("Internal server error (number of sentences are different from translations)");
      int MaxLineNumber = -1;
      for(size_t i = 0; i < LineNumbers.size(); ++i)
        if(MaxLineNumber < LineNumbers[i])
          MaxLineNumber = LineNumbers[i];
      std::vector<std::vector<std::string>> FinalTokens;
      std::vector<decltype(Results)> FinalPhrases;
      FinalTokens.resize(MaxLineNumber + 1);
      FinalPhrases.resize(MaxLineNumber + 1);
      for(size_t i = 0; i < Sentences.size(); ++i) {
        int LineNo = LineNumbers[i];
        FinalTokens[LineNo].emplace_back(std::move(Sentences[i]));
        FinalPhrases[LineNo].emplace_back(std::move(Results[i]));
      }

      nlohmann::json ResponseJSON;
      auto& Rslt = ResponseJSON["rslt"] = nlohmann::json::array();
      for(int i = 0; i <= MaxLineNumber; ++i) {
        nlohmann::json RsltItem;
        RsltItem["tokens"] = FinalTokens[i];
        RsltItem["phrases"] = FinalPhrases[i];
        std::vector<std::vector<int>> Alignments;
        Alignments.resize(FinalPhrases[i].size());
       for(size_t j = 0; j < FinalPhrases[i].size(); ++j)
          Alignments[j].push_back({ static_cast<int>(j) });
        RsltItem["alignments"] = Alignments;
        Rslt.push_back(RsltItem);
      }
      ResponseJSON["serverName"] = ServerName;
      auto ResponseJsonString = ResponseJSON.dump();
      _response << ResponseJsonString.c_str();
      _response.send();
      LOG(info, "[rest server] Request answered.");
    });
  }
  Server.start();
  return 0;
}
