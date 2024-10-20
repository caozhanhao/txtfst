#ifndef TXTFST_FST_H
#define TXTFST_FST_H
#pragma once

#include <string>
#include <optional>
#include <concepts>
#include <vector>
#include <unordered_set>
#include <memory>
#include <cassert>
#include <sys/stat.h>

#include "packme/packme.h"

namespace txtfst
{
  template<std::integral Output>
  struct State
  {
    struct Arc
    {
      char label{0};
      size_t id{0};
      Output output{0};

      bool operator==(const Arc& rhs) const
      {
        return label == rhs.label && id == rhs.id && output == rhs.output;
      }
    };

    size_t id{0};
    bool final{false};
    std::vector<Arc> trans;

    void set_arc(char label, size_t id)
    {
      if (auto it = std::ranges::find(trans, label, [](auto&& t) { return t.label; }); it != trans.cend())
      {
        it->id = id;
      }
      else
      {
        trans.emplace_back(label, id, 0);
      }
    }

    Output& output(char label)
    {
      auto it = std::ranges::find(trans, label, [](auto&& t) { return t.label; });
      assert(it != trans.cend());
      return it->output;
    }
  };

  template<std::integral Output>
  struct CompiledFSTView
  {
    std::vector<size_t> jump_table;
    const char* fst{nullptr};
    size_t size{0};

    std::optional<Output> get(const std::string& word) const
    {
      Output output = 0;
      auto curr = get_state(0);
      for (auto& ch : word)
      {
        auto it = std::ranges::find(curr.trans, ch, [](auto&& t) { return t.label; });
        if (it == curr.trans.cend())
          return std::nullopt;
        output += it->output;
        curr = get_state(it->id);
      }
      if (!curr.final)
        return std::nullopt;
      return output;
    }

  private:
    State<Output> get_state(size_t index) const
    {
      assert(fst != nullptr && index < jump_table.size());
      State<Output> dest;
      auto curr = fst + jump_table[index];
      size_t curr_len;
      if(index != jump_table.size() - 1)
        curr_len = fst + jump_table[index + 1] - curr;
      else
        curr_len = size;

      std::memcpy(&dest.id, curr, sizeof(dest.id));
      std::memcpy(&dest.final, curr + sizeof(dest.id), sizeof(dest.final));
      for(auto arc = curr + sizeof(dest.id) + sizeof(dest.final); arc < curr + curr_len;)
      {
        dest.trans.emplace_back();
        std::memcpy(&dest.trans.back(), arc, sizeof(typename State<Output>::Arc));
        arc += sizeof(typename State<Output>::Arc);
      }
      return dest;
    }
  };

  template<std::integral Output>
  struct FST
  {
    std::vector<State<Output> > states;

    [[nodiscard]] std::tuple<std::vector<char>, std::vector<size_t>> compile() const
    {
      std::vector<size_t> jump_table;
      jump_table.resize(states.size());
      std::vector<char> fst;
      fst.reserve(states.size() * sizeof(State<Output>) * 2);
      size_t offset = 0;
      for(size_t i = 0; i < states.size(); ++i)
      {
        const auto& state = states[i];

        size_t expected = sizeof(state.id) + sizeof(state.final) + state.trans.size() * sizeof(typename State<Output>::Arc);
        if(fst.size() - offset < expected)
          fst.resize(fst.size() + expected * 2);

        jump_table[state.id] = offset;
        std::memcpy(fst.data() + offset, &state.id, sizeof(state.id));
        offset += sizeof(state.id);
        std::memcpy(fst.data() + offset, &state.final, sizeof(state.final));
        offset += sizeof(state.final);
        for(auto&& arc : state.trans)
        {
          std::memcpy(fst.data() + offset, &arc, sizeof(arc));
          offset += sizeof(arc);
        }
      }
      fst.resize(offset);
      return {fst, jump_table};
    }
  };

  template<std::integral Output>
  class FSTBuilder
  {
  public:
    enum class AddRet
    {
      Success,
      EmptyWord,
      DuplicateWord,
      UnsortedWord,
    };

  private:
    using StatePtr = std::shared_ptr<State<Output> >;
    using state_hash = decltype([](auto&& state) -> size_t
    {
      return std::hash<std::string>{}(packme::pack(state->trans));
    });
    using state_eq = decltype([](auto&& a, auto&& b) -> size_t { return a->trans == b->trans; });
    size_t next_state_id;
    std::string prev_word;
    std::unordered_set<StatePtr, state_hash, state_eq> freezed;
    std::vector<StatePtr> frontier;

  public:
    FSTBuilder(): next_state_id(0)
    {
      frontier.emplace_back(std::make_shared<State<Output> >(State<Output>{.id = next_state_id++}));
    }

    AddRet add(const std::string& word, Output output)
    {
      if (word.empty()) return AddRet::EmptyWord;
      if (word == prev_word) return AddRet::DuplicateWord;

      size_t common_prefix_size = 0;
      for (size_t i = 0; i < word.size() && i < prev_word.size(); ++i)
      {
        if (word[i] == prev_word[i])
          ++common_prefix_size;
        else if (word[i] > prev_word[i])
          break;
        else if (word[i] < prev_word[i])
          return AddRet::UnsortedWord;
      }

      // First freeze all the states after the common prefix.
      for (size_t i = prev_word.size(); i > common_prefix_size; --i)
      {
        auto it = freezed.find(frontier[i]);
        if (it == freezed.end())
        {
          freezed.insert(frontier[i]);
          frontier[i - 1]->set_arc(prev_word[i - 1], frontier[i]->id);
          frontier[i] = nullptr;
        }
        else
        {
          --next_state_id;
          frontier[i - 1]->set_arc(prev_word[i - 1], (**it).id);
        }
      }

      // Then create arcs to new suffix states.
      for (size_t i = common_prefix_size; i < word.size(); ++i)
      {
        if (i + 1 == frontier.size())
          frontier.emplace_back(nullptr);
        frontier[i + 1] = std::make_shared<State<Output> >(State<Output>{.id = next_state_id++});
        frontier[i]->set_arc(word[i], frontier[i + 1]->id);
      }
      frontier[word.size()]->final = true;

      // Finally we handle the outputs
      auto curr_output = output;
      for (size_t i = 1; i <= common_prefix_size; ++i)
      {
        StatePtr prev_state = frontier[i - 1];
        Output& prev_output = prev_state->output(word[i - 1]);
        auto prefix = (std::min)(prev_output, curr_output);
        auto suffix = prev_output - prefix;
        prev_output = prefix;

        if (suffix != 0)
        {
          for (auto&& arc : frontier[i]->trans)
          {
            arc.output += suffix;
          }
        }
        curr_output -= prefix;
      }

      frontier[common_prefix_size]->output(word[common_prefix_size]) = curr_output;

      prev_word = word;
      return AddRet::Success;
    }

    FST<Output> build()
    {
      for (int i = static_cast<int>(prev_word.size()); i >= 0; --i)
      {
        auto it = freezed.find(frontier[i]);
        if (it == freezed.end())
        {
          freezed.insert(frontier[i]);
        }
        else
        {
          --next_state_id;
          if (i - 1 > 0)
            frontier[i - 1]->set_arc(prev_word[i - 1], (**it).id);
        }
      }
      frontier.clear();


      std::vector<State<Output> > ret;
      for (auto&& ptr : freezed)
      {
        assert(ptr != nullptr);
        ret.emplace_back(std::move(*ptr));
      }
      freezed.clear();
      std::ranges::sort(ret, std::less{}, [](auto&& r) { return r.id; });
      return FST<Output>{ret};
    }

    // void debug()
    // {
    //   std::vector<StatePtr> dbg;
    //   for(auto&& ptr : freezed)
    //   {
    //     assert(ptr != nullptr);
    //     dbg.emplace_back(ptr);
    //   }
    //   std::ranges::sort(dbg, std::less{}, [](auto&& r){return r->id;});
    //   for(auto&& ptr : dbg)
    //   {
    //     std::cout << "STATE: " << ptr->id;
    //     if(ptr->final) std::cout << " | FINAL";
    //     std::cout << "\n";
    //     for(auto& arc : ptr->trans)
    //     {
    //       std::cout << "    " << arc.label << " -> " << arc.id;
    //       if(arc.output != 0) std::cout << " | OUTPUT: " << arc.output;
    //       std::cout << "\n";
    //     }
    //   }
    // }
  };
}
#endif
