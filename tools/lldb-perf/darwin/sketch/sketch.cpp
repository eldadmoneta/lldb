//===-- sketch.cpp ----------------------------------------------*- C++ -*-===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//

#include <CoreFoundation/CoreFoundation.h>

#include "lldb-perf/lib/Timer.h"
#include "lldb-perf/lib/Metric.h"
#include "lldb-perf/lib/Measurement.h"
#include "lldb-perf/lib/TestCase.h"
#include "lldb-perf/lib/Xcode.h"

#include <iostream>
#include <unistd.h>
#include <fstream>

using namespace lldb_perf;

class SketchTest : public TestCase
{
public:
    SketchTest () :
        m_fetch_frames_measurement ([this] () -> void
            {
                Xcode::FetchFrames (GetProcess(),false,false);
            }, "fetch-frames", "time to dump backtrace for every frame in every thread"),
        m_file_line_bp_measurement([this] (const char* file, uint32_t line) -> void
            {
                Xcode::CreateFileLineBreakpoint(GetTarget(), file, line);
            }, "file-line-bkpt", "time to set a breakpoint given a file and line"),
        m_fetch_modules_measurement ([this] () -> void
            {
                Xcode::FetchModules(GetTarget());
            }, "fetch-modules", "time to get info for all modules in the process"),
        m_fetch_vars_measurement([this] (int depth) -> void
            {
                SBProcess process (GetProcess());
                auto threads_count = process.GetNumThreads();
                for (size_t thread_num = 0; thread_num < threads_count; thread_num++)
                {
                    SBThread thread(process.GetThreadAtIndex(thread_num));
                    SBFrame frame(thread.GetFrameAtIndex(0));
                    Xcode::FetchVariables(frame,depth,GetVerbose());
                }
            }, "fetch-vars", "time to dump variables for the topmost frame in every thread"),
        m_run_expr_measurement([this] (SBFrame frame, const char* expr) -> void
            {
                SBValue value(frame.EvaluateExpression(expr, lldb::eDynamicCanRunTarget));
                Xcode::FetchVariable (value, 0, GetVerbose());
            }, "run-expr", "time to evaluate an expression and display the result")
    {}
    
    virtual
    ~SketchTest ()
    {
    }
    
    virtual bool
	Setup (int argc, const char** argv)
    {
        //SetVerbose(true);
        m_app_path.assign(argv[1]);
        m_doc_path.assign(argv[2]);
        m_out_path.assign(argv[3]);
        TestCase::Setup(argc,argv);
        m_target = m_debugger.CreateTarget(m_app_path.c_str());
        const char* file_arg = m_doc_path.c_str(); 
        const char* persist_arg = "-ApplePersistenceIgnoreState";
        const char* persist_skip = "YES";
        const char* empty = nullptr;
        const char* args[] = {file_arg,persist_arg,persist_skip,empty};
        SBLaunchInfo launch_info (args);
        m_file_line_bp_measurement("SKTDocument.m",245);
        m_file_line_bp_measurement("SKTDocument.m",283);
        m_file_line_bp_measurement("SKTText.m",326);
        return Launch (launch_info);
    }
    
    void
    DoTest ()
    {
        m_fetch_frames_measurement();
        m_fetch_modules_measurement();
        m_fetch_vars_measurement(1);
    }
    
	virtual void
	TestStep (int counter, ActionWanted &next_action)
    {
        switch (counter)
        {
        case 0:
            {
                DoTest ();
                m_file_line_bp_measurement("SKTDocument.m",254);
                next_action.Continue();
            }
            break;
                
        case 1:
            {
                DoTest ();
                m_run_expr_measurement(m_thread.GetFrameAtIndex(0),"properties");
                m_run_expr_measurement(m_thread.GetFrameAtIndex(0),"[properties description]");
                m_run_expr_measurement(m_thread.GetFrameAtIndex(0),"typeName");
                m_run_expr_measurement(m_thread.GetFrameAtIndex(0),"data");
                m_run_expr_measurement(m_thread.GetFrameAtIndex(0),"[data description]");
                next_action.Continue();
            }
            break;

        case 2:
            {
                DoTest ();
                next_action.Continue();
            }
            break;

        case 3:
            {
                DoTest ();
                next_action.StepOver(m_thread);
            }
            break;

        case 4:
            {
                DoTest ();
                m_run_expr_measurement(m_thread.GetFrameAtIndex(0),"layoutManager");
                m_run_expr_measurement(m_thread.GetFrameAtIndex(0),"contents");
                next_action.StepOver(m_thread);
            }
            break;
        
        case 5:
            {
                DoTest ();
                next_action.StepOver(m_thread);
            }
            break;

        case 6:
            {
                DoTest ();
                next_action.StepOver(m_thread);
            }
            break;

        case 7:
            {
                DoTest ();
                m_run_expr_measurement(m_thread.GetFrameAtIndex(0),"@\"an NSString\"");
                m_run_expr_measurement(m_thread.GetFrameAtIndex(0),"[(id)@\"an NSString\" description]");
                m_run_expr_measurement(m_thread.GetFrameAtIndex(0),"@[@1,@2,@3]");
                next_action.StepOut(m_thread);
            }
            break;

        case 8:
            {
                DoTest ();
                m_run_expr_measurement(m_thread.GetFrameAtIndex(0),"[graphics description]");
                m_run_expr_measurement(m_thread.GetFrameAtIndex(0),"[selectionIndexes description]");
                m_run_expr_measurement(m_thread.GetFrameAtIndex(0),"(BOOL)NSIntersectsRect(rect, graphicDrawingBounds)");
                next_action.Kill();
            }
            break;
        
        default:
            {
                next_action.Kill();
            }
            break;
        }
    }
    
    virtual void
    WriteResults (Results &results)
    {
        m_fetch_frames_measurement.WriteAverageValue(results);
        m_file_line_bp_measurement.WriteAverageValue(results);
        m_fetch_modules_measurement.WriteAverageValue(results);
        m_fetch_vars_measurement.WriteAverageValue(results);
        m_run_expr_measurement.WriteAverageValue(results);
        results.Write(m_out_path.c_str());
    }
    
private:
    Measurement<lldb_perf::TimeGauge, std::function<void()>> m_fetch_frames_measurement;
    Measurement<lldb_perf::TimeGauge, std::function<void(const char*, uint32_t)>> m_file_line_bp_measurement;
    Measurement<lldb_perf::TimeGauge, std::function<void()>> m_fetch_modules_measurement;
    Measurement<lldb_perf::TimeGauge, std::function<void(int)>> m_fetch_vars_measurement;
    Measurement<lldb_perf::TimeGauge, std::function<void(SBFrame, const char*)>> m_run_expr_measurement;
    
    std::string m_app_path;
    std::string m_doc_path;
    std::string m_out_path;
};

// argv[1] == path to app
// argv[2] == path to document
// argv[3] == path to result
int main(int argc, const char * argv[])
{
    SketchTest skt;
    TestCase::Run(skt,argc,argv);
    return 0;
}

