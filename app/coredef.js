.pragma library

const StateOk = 0;
const StateFail = 1;
const StateFuture = 2;
const StateCancel = 3;

const CmdReply = "reply"
const CmdQueryPromise = 'queryPromise'
const CmdCancelPromise = 'cancelPromise'
const CmdGetLines = 'getLines'
const CmdOpenFile = 'openFile'
const CmdFilter = 'filter'
const CmdSyncLogs = 'syncLogs'
const ServerCmdRangeChanged = 'rangeChanged'
