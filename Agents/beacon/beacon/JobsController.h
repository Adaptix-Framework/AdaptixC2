#pragma once

#include "std.cpp"
#include "Packer.h"

#define COMMAND_JOB 0x8437

#define JOB_TYPE_LOCAL   0x1
#define JOB_TYPE_REMOTE  0x2
#define JOB_TYPE_PROCESS 0x3

#define JOB_STATE_RUNNING  0x1
#define JOB_STATE_FINISHED 0x2
#define JOB_STATE_KILLED   0x3

struct JobData {
    ULONG  jobId;
    WORD   jobType;
    WORD   jobState;
    HANDLE jobObject;
    WORD   pidObject;
    HANDLE pipeRead;
    HANDLE pipeWrite;
};

class JobsController
{
public:
	Vector<JobData> jobs;

    JobData CreateJobData(ULONG taskId, WORD Type, WORD State, HANDLE object, WORD pid, HANDLE input, HANDLE output);
    void    ProcessJobs(Packer* packer);
};