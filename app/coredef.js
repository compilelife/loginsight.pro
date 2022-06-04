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
const CmdOpenProcess = 'openProcess'
const CmdFilter = 'filter'
const CmdSyncLogs = 'syncLogs'
const CmdSetLineSegment = 'setLineSegment'
const CmdSearch = 'search'
const CmdMapLine = 'mapLine'
const ServerCmdRangeChanged = 'rangeChanged'

const SegTypeDate = 0;
const SegTypeLogLevel = 1;
const SegTypeNum = 2;
const SegTypeStr = 3;

const LogLevelDebug = 0;
const LogLevelInfo = 1;
const LogLevelWarn = 2;
const LogLevelError = 3;
const LogLevelFatal = 4;
