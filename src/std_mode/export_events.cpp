#include "../core/common.h"

#include "../core/api.h"
#include "../core/event.h"
#include "../core/init.h"
#include "../utils/function.h"
#include "../utils/string.h"

#define CQ_EVENT(ReturnType, Name, Size)                                                              \
    __pragma(comment(linker, "/EXPORT:" #Name "=_" #Name "@" #Size)) extern "C" __declspec(dllexport) \
        ReturnType __stdcall Name

using namespace std;
using namespace cq;
using utils::call_all;
using utils::string_from_coolq;

// TODO: 本文件需要 review 一下

#pragma region Lifecycle

/**
 * 返回 API 版本和 App Id.
 */
CQ_EVENT(const char *, AppInfo, 0)
() { return "9," APP_ID; }

/**
 * 生命周期: 初始化.
 */
CQ_EVENT(int32_t, Initialize, 4)
(const int32_t auth_code) {
    __ac = auth_code;
    __init();
    __init_api();
    call_all(_initialize_callbacks);
    return 0;
}

/**
 * 生命周期: 插件启用.
 */
CQ_EVENT(int32_t, cq_enable, 0)
() {
    call_all(_enable_callbacks);
    return 0;
}

/**
 * 生命周期: 插件停用.
 */
CQ_EVENT(int32_t, cq_disable, 0)
() {
    call_all(_disable_callbacks);
    return 0;
}

/**
 * 生命周期: 酷Q启动.
 */
CQ_EVENT(int32_t, cq_coolq_start, 0)
() {
    call_all(_coolq_start_callbacks);
    return 0;
}

/**
 * 生命周期: 酷Q退出.
 */
CQ_EVENT(int32_t, cq_coolq_exit, 0)
() {
    call_all(_coolq_exit_callbacks);
    return 0;
}

#pragma endregion

#pragma region Message

/**
 * Type=21 私聊消息
 * sub_type 子类型，11/来自好友 1/来自在线状态 2/来自群 3/来自讨论组
 */
CQ_EVENT(int32_t, cq_event_private_msg, 24)
(int32_t sub_type, int32_t msg_id, int64_t from_qq, const char *msg, int32_t font) {
    using SubType = PrivateMessageEvent::SubType;
    PrivateMessageEvent e;
    e.time = time(nullptr);
    switch (sub_type) {
    case 11:
        e.sub_type = SubType::FRIEND;
        break;
    case 2:
        e.sub_type = SubType::GROUP;
        break;
    case 3:
        e.sub_type = SubType::DISCUSS;
        break;
    case 1:
        e.sub_type = SubType::OTHER;
        break;
    default:
        e.sub_type = SubType::UNKNOWN;
        break;
    }
    e.target = Target(from_qq);
    e.message_id = msg_id;
    e.raw_message = string_from_coolq(msg);
    e.message = e.raw_message;
    e.font = font;
    e.user_id = from_qq;
    call_all(_private_message_callbacks, e);
    return e.operation;
}

/**
 * Type=2 群消息
 */
CQ_EVENT(int32_t, cq_event_group_msg, 36)
(int32_t sub_type, int32_t msg_id, int64_t from_group, int64_t from_qq, const char *from_anonymous, const char *msg,
 int32_t font) {
    GroupMessageEvent e;
    e.time = time(nullptr);
    e.sub_type = GroupMessageEvent::SubType::DEFAULT;
    e.target = Target(from_qq, from_group, Target::GROUP);
    e.message_id = msg_id;
    e.raw_message = string_from_coolq(msg);
    e.message = e.raw_message; // TODO: 处理 Air 的匿名消息特殊情况
    e.font = font;
    e.user_id = from_qq;
    e.group_id = from_group;
    call_all(_group_message_callbacks, e);
    return e.operation;
}

/**
 * Type=4 讨论组消息
 */
CQ_EVENT(int32_t, cq_event_discuss_msg, 32)
(int32_t sub_type, int32_t msg_id, int64_t from_discuss, int64_t from_qq, const char *msg, int32_t font) {
    DiscussMessageEvent e;
    e.time = time(nullptr);
    e.sub_type = DiscussMessageEvent::SubType::DEFAULT;
    e.target = Target(from_qq, from_discuss, Target::DISCUSS);
    e.message_id = msg_id;
    e.raw_message = string_from_coolq(msg);
    e.message = e.raw_message;
    e.font = font;
    e.user_id = from_qq;
    e.discuss_id = from_discuss;
    call_all(_discuss_message_callbacks, e);
    return e.operation;
}

#pragma endregion

#pragma region Notice

/**
 * Type=11 群事件-文件上传
 */
CQ_EVENT(int32_t, cq_event_group_upload, 28)
(int32_t sub_type, int32_t send_time, int64_t from_group, int64_t from_qq, const char *file) {
    GroupUploadEvent e;
    e.time = time(nullptr);
    e.sub_type = GroupUploadEvent::SubType::DEFAULT;
    e.target = Target(from_qq, from_group, Target::GROUP);
    // TODO: e.file
    e.user_id = from_qq;
    e.group_id = from_group;
    call_all(_group_upload_callbacks, e);
    return e.operation;
}

/**
 * Type=101 群事件-管理员变动
 * sub_type 子类型，1/被取消管理员 2/被设置管理员
 */
CQ_EVENT(int32_t, cq_event_group_admin, 24)
(int32_t sub_type, int32_t send_time, int64_t from_group, int64_t being_operate_qq) {
    using SubType = GroupAdminEvent::SubType;
    GroupAdminEvent e;
    e.time = time(nullptr);
    switch (sub_type) {
    case 1:
        e.sub_type = SubType::UNSET;
        break;
    case 2:
        e.sub_type = SubType::SET;
        break;
    default:
        e.sub_type = SubType::UNKNOWN;
        break;
    }
    e.target = Target(being_operate_qq, from_group, Target::GROUP);
    e.user_id = being_operate_qq;
    e.group_id = from_group;
    call_all(_group_admin_callbacks, e);
    return e.operation;
}

/**
 * Type=102 群事件-群成员减少
 * sub_type 子类型，1/群员离开 2/群员被踢 3/自己(即登录号)被踢
 * from_qq 操作者QQ(仅subType为2、3时存在)
 * being_operate_qq 被操作QQ
 */
CQ_EVENT(int32_t, cq_event_group_member_decrease, 32)
(int32_t sub_type, int32_t send_time, int64_t from_group, int64_t from_qq, int64_t being_operate_qq) {
    using SubType = GroupMemberDecreaseEvent::SubType;
    GroupMemberDecreaseEvent e;
    e.time = time(nullptr);
    switch (sub_type) {
    case 1:
        e.sub_type = SubType::LEAVE;
        break;
    case 2:
        e.sub_type = SubType::KICK;
        break;
    case 3:
        e.sub_type = SubType::KICK_ME; // TODO: 检查 kick 子类型的 being_operate_qq
        break;
    default:
        e.sub_type = SubType::UNKNOWN;
        break;
    }
    e.target = Target(being_operate_qq, from_group, Target::GROUP);
    e.user_id = being_operate_qq;
    e.group_id = from_group;
    e.operator_id = e.sub_type == SubType::LEAVE ? being_operate_qq : from_qq;
    call_all(_group_member_decrease_callbacks, e);
    return e.operation;
}

/**
 * Type=103 群事件-群成员增加
 * sub_type 子类型，1/管理员已同意 2/管理员邀请
 * from_qq 操作者QQ(即管理员QQ)
 * being_operate_qq 被操作QQ(即加群的QQ)
 */
CQ_EVENT(int32_t, cq_event_group_member_increase, 32)
(int32_t sub_type, int32_t send_time, int64_t from_group, int64_t from_qq, int64_t being_operate_qq) {
    using SubType = GroupMemberIncreaseEvent::SubType;
    GroupMemberIncreaseEvent e;
    e.time = time(nullptr);
    switch (sub_type) {
    case 1:
        e.sub_type = SubType::APPROVE;
        break;
    case 2:
        e.sub_type = SubType::INVITE;
        break;
    default:
        e.sub_type = SubType::UNKNOWN;
        break;
    }
    e.target = Target(being_operate_qq, from_group, Target::GROUP);
    e.user_id = being_operate_qq;
    e.group_id = from_group;
    e.operator_id = from_qq;
    call_all(_group_member_increase_callbacks, e);
    return e.operation;
}

/**
 * Type=104 群事件-群禁言
 * sub_type 子类型，1/被解禁 2/被禁言
 * from_group 来源群号
 * from_qq 操作者QQ(即管理员QQ)
 * being_operate_qq 被操作QQ(若为全群禁言/解禁，则本参数为 0)
 * duration 禁言时长(单位 秒，仅子类型为2时可用)
 */
CQ_EVENT(int32_t, cq_event_group_ban, 40)
(int32_t sub_type, int32_t send_time, int64_t from_group, int64_t from_qq, int64_t being_operate_qq, int64_t duration) {
    using SubType = GroupBanEvent::SubType;
    GroupBanEvent e;
    e.time = time(nullptr);
    switch (sub_type) {
    case 1:
        e.sub_type = SubType::LIFT_BAN;
        break;
    case 2:
        e.sub_type = SubType::BAN;
        break;
    default:
        e.sub_type = SubType::UNKNOWN;
        break;
    }
    e.target = Target(being_operate_qq, from_group, Target::GROUP);
    e.user_id = being_operate_qq;
    e.group_id = from_group;
    e.operator_id = from_qq;
    e.duration = duration;
    call_all(_group_ban_callbacks, e);
    return e.operation;
}

/**
 * Type=201 好友事件-好友已添加
 */
CQ_EVENT(int32_t, cq_event_friend_add, 16)
(int32_t sub_type, int32_t send_time, int64_t from_qq) {
    FriendAddEvent e;
    e.time = time(nullptr);
    e.sub_type = FriendAddEvent::SubType::DEFAULT;
    e.target = Target(from_qq);
    e.user_id = from_qq;
    call_all(_friend_add_callbacks, e);
    return e.operation;
}

#pragma endregion

#pragma region Request

/**
 * Type=301 请求-好友添加
 * msg 附言
 * response_flag 反馈标识(处理请求用)
 */
CQ_EVENT(int32_t, cq_event_friend_request, 24)
(int32_t sub_type, int32_t send_time, int64_t from_qq, const char *msg, const char *response_flag) {
    FriendRequestEvent e;
    e.time = time(nullptr);
    e.sub_type = FriendRequestEvent::SubType::DEFAULT;
    e.target = Target(from_qq);
    e.comment = string_from_coolq(msg);
    e.flag = string_from_coolq(response_flag);
    e.user_id = from_qq;
    call_all(_friend_request_callbacks, e);
    return e.operation;
}

/**
 * Type=302 请求-群添加
 * sub_type 子类型，1/他人申请入群 2/自己(即登录号)受邀入群
 * msg 附言
 * response_flag 反馈标识(处理请求用)
 */
CQ_EVENT(int32_t, cq_event_group_request, 32)
(int32_t sub_type, int32_t send_time, int64_t from_group, int64_t from_qq, const char *msg, const char *response_flag) {
    using SubType = GroupRequestEvent::SubType;
    GroupRequestEvent e;
    e.time = time(nullptr);
    switch (sub_type) {
    case 1:
        e.sub_type = SubType::ADD;
        break;
    case 2:
        e.sub_type = SubType::INVITE;
        break;
    default:
        e.sub_type = SubType::UNKNOWN;
        break;
    }
    e.target = Target(from_qq, from_group, Target::GROUP);
    e.comment = string_from_coolq(msg);
    e.flag = string_from_coolq(response_flag);
    e.user_id = from_qq;
    e.group_id = from_group;
    call_all(_group_request_callbacks, e);
    return e.operation;
}

#pragma endregion