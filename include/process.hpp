#pragma once

#include <stdbool.h>
#include <wchar.h>

#include <array>

#include <defjams.h>
#include <interpreter/object.hpp>
#include <pattern_functions.hpp>
#include <pattern_parser/ast.hpp>
#include <pattern_parser/parser.hpp>
#include <pattern_parser/tokenizer.hpp>
#include <sequenceengine.h>

struct ProcessConfig
{
    ProcessType process_type;
    ProcessTimerType timer_type;
    float loop_len;
    std::string command;
    ProcessPatternTarget target_type;
    std::vector<std::string> targets;
    std::string pattern;
    std::shared_ptr<object::Object> generator;
    std::vector<std::shared_ptr<PatternFunction>> funcz;
    mixer_timing_info tinfo;
};

class Process
{
  public:
    Process() = default;
    ~Process();
    std::string Status();
    void Start();
    void Stop();
    void EventNotify(mixer_timing_info);
    void SetDebug(bool b);
    void SetSpeed(float val);
    void ParsePattern();
    void EnqueueUpdate(ProcessConfig config);
    void Update();
    void
    EvalPattern(std::shared_ptr<pattern_parser::PatternNode> const &pattern,
                int target_start, int target_end);
    void AppendPatternFunction(std::shared_ptr<PatternFunction> func);

  public:
    ProcessConfig pending_config_;
    bool update_pending_;

    ProcessType process_type_{ProcessType::NO_PROCESS_TYPE};

    // Command Process Vars
    ProcessTimerType timer_type_{ProcessTimerType::NO_PROCESS_TIMER_TYPE};
    float loop_len_{1};
    std::string command_{};

    // Pattern Process Vars
    ProcessPatternTarget target_type_{
        ProcessPatternTarget::NO_PROCESS_PATTERN_TARGET};
    std::vector<std::string> targets_;

    // Timer vars
    float start_{0};
    float end_{0};
    float incr_{0};
    float current_val_{0};

    std::string pattern_;
    std::shared_ptr<object::Object> generator_;

    bool started_;
    bool active_;
    bool debug_;

    // While varz
    std::string while_condition_;
    std::string while_body_;
    std::string while_then_body_;
    // Expression

  private:
    std::shared_ptr<pattern_parser::PatternNode> pattern_root_;
    std::array<std::vector<std::shared_ptr<MusicalEvent>>, PPBAR>
        pattern_events_;

    // speed controls
    std::array<bool, PPBAR> pattern_events_played_ = {}; // for slow speed
    float cur_event_idx_{0};
    float event_incr_speed_{1};

    std::vector<std::shared_ptr<PatternFunction>> pattern_functions_;
    int loop_counter_;
};
