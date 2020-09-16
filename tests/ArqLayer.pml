#define false 0
#define true 1

// Limit the sequence numbers somewhat. Our request are small (max 4 chunks),
// so this value is safe, and makes the verifier faster.
#define MAX_SEQ 15
#define nextSeq(x) (((x) % MAX_SEQ) + 1)

mtype = {
	idle,
	// target states
	decoding, decoded, retransmit,
}

proctype target(chan i; chan o)
{
	mtype state = idle;
	int seq;
	bool rst = false;
	bool last;
	int decodeSeq = 1;
	int decodeSeqStart;
	int encodeSeq;
	bool encodeRst;

	do
	::	if
		:: rst ->
			printf("target reset at %d\n", seq);
			decodeSeq = nextSeq(seq);
			encodeSeq = 0;
			encodeRst = true;
			state = idle;
			o!0,true,true;
			goto recv;
		:: rst == false && state == idle && seq == decodeSeq ->
			state = decoding;
			decodeSeqStart = decodeSeq;
		:: rst == false && state == decoding && seq == decodeSeq ->
			if
			:: last ->
				// Reply.
				// Only reply with one segment, as there is no difference in sending one or multiple.
				// If something is lost, all are retransmitted anyway.
				encodeSeq = nextSeq(encodeSeq);
				printf("target response %d -> %d (%d)\n", seq, encodeSeq, encodeRst)
				o!encodeSeq,encodeRst,true;
				state = decoded;
			:: last ->
				// Reply with reset seq.
				encodeRst = true;
				encodeSeq = nextSeq(encodeSeq);
				printf("target response %d -> %d (%d)\n", seq, encodeSeq, encodeRst)
				o!encodeSeq,encodeRst,true;
				state = decoded;
			:: !last ->
				printf("target got partial request %d\n", seq)
			fi
			decodeSeq = nextSeq(decodeSeq);
			goto recv;
		:: rst == false && state == decoded && seq == decodeSeq ->
			// Prepare for next request.
			state = idle;
			encodeRst = false;
		:: rst == false && (state == decoded || state == retransmit) && seq == decodeSeqStart ->
			state = retransmit;
			printf("target prepare retransmit\n");
		:: rst == false && state == retransmit && nextSeq(seq) == decodeSeq ->
			assert(last)
			// Retransmit previous request.
			printf("target retransmit %d -> %d (%d)\n", seq, encodeSeq, encodeRst);
			o!encodeSeq,encodeRst,true;
			state = decoded
			goto recv;
		fi
	:: else ->
recv:
		printf("target waiting for request %d\n", decodeSeq);
end:
		i?seq,rst,last
	od
}

proctype lossy(chan i; chan o)
{
	int seq;
	bool rst;
	bool last;

end:
	do
	:: i?seq,rst,last -> o!seq,rst,last;
	:: i?seq,rst,last -> skip; // packet lost
	od;
}

proctype client(chan i; chan o; int msg)
{
	int seq, seqStart, seqEnd, targetSeq;
	bool rst;
	bool last;

reset:
	seqStart = nextSeq(seqStart);
	seqEnd = seqStart;
	rst = true;

	// Transmit full request
resend:
	seq = seqStart;
	do
	:: if
		:: seq == seqStart && seq == seqEnd ->
			printf("client request %d (%d)\n", seq, rst);
			o!seq,rst,true;
			break;
		:: seq == seqStart && seq != seqEnd ->
			printf("client partial request %d (%d)\n", seq, rst);
			o!seq,rst,false;
		:: seq != seqStart && seq == seqEnd ->
			printf("client end request %d\n", seq);
			o!seq,false,true;
			break;
		:: else ->
			printf("client partial request %d\n", seq);
			o!seq,false,false;
		fi;
		seq = nextSeq(seq);
	od

	// Wait for response
	if
	:: i?seq,rst,last ->
		printf("client got %d (%d)\n", seq, rst);
		assert(last);
		if
		:: rst ->
			targetSeq = nextSeq(seq);
		:: else ->
			assert(seq == targetSeq);
			targetSeq = nextSeq(targetSeq);
		fi;
	:: timeout ->
		printf("client resend\n");
		goto resend;
	:: timeout ->
		// Arbitrary reset in the middle of the communication.
		printf("client reset\n");
		if
		:: !rst -> msg = msg + 1;
		:: else -> skip;
		fi;
		goto reset;
	fi;

	if
	:: msg > 0 ->
		msg = msg - 1;
		seqStart = nextSeq(seqEnd);
		if
		:: seqEnd = seqStart;
		:: seqEnd = nextSeq(seqStart);
		:: seqEnd = nextSeq(nextSeq(seqStart));
		:: seqEnd = nextSeq(nextSeq(nextSeq(seqStart)));
		fi;
		rst = false;
		goto resend;
	:: else -> skip;
	fi;
}

init
{
	// message: seq, reset flag, last flag
	chan i1 = [0] of { int, bool, bool };
	chan i2 = [0] of { int, bool, bool };
	chan o1 = [0] of { int, bool, bool };
	chan o2 = [0] of { int, bool, bool };
	run target(i1, o1);
	run lossy(o2, i1);
	run lossy(o1, i2);
	run client(i2, o2, 3);
}

