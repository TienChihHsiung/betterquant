#!/usr/bin/python3

# -*- coding: utf-8 -*-
import sys
import getopt
import datetime
import time
import json
from stgeng import *

sys.path.append(".")


class StgInstTaskHandler(StgInstTaskHandlerBase):
    def on_stg_start(self):
        # Install a timer for stg inst 1 that fires 1 time after a while
        self.stg_eng.install_stg_inst_timer(
            stg_inst_id=1,
            timer_name="TestRealOrder",
            millisec_interval=5000,
            exec_at_startup=ExecAtStartup.IsFalse,
            max_exec_times=1,
        )

        # Install a timer for stg inst 1 that fires 1 time after a while
        self.stg_eng.install_stg_inst_timer(
            stg_inst_id=1,
            timer_name="TestSimedOrder",
            exec_at_startup=ExecAtStartup.IsFalse,
            millisec_interval=1000 * 3600 * 24,
            max_exec_times=1,
        )

        # Install a timer for stg inst 1 that fires 1 time after a while
        self.stg_eng.install_stg_inst_timer(
            stg_inst_id=1,
            timer_name="TestCancelOrder",
            millisec_interval=1000 * 3600 * 24,
            exec_at_startup=ExecAtStartup.IsFalse,
            max_exec_times=1,
        )

        self.__query_his_md()

    def __query_his_md(self):
        return
        ret_of_qry = self.stg_eng.query_specific_num_of_his_md_after_ts(
            topic="MD@SZSE@Spot@000001@Tickers", ts=1672363452000000, num=2
        )

        ret_of_qry = self.stg_eng.query_specific_num_of_his_md_before_ts(
            topic="MD@SZSE@Spot@000001@Tickers", ts=1672363452000000, num=2
        )

        ret_of_qry = self.stg_eng.query_his_md_between_2_ts(
            topic="MD@SZSE@Spot@000001@Tickers",
            ts_begin=1672363452000000,
            ts_end=1672363470000000,
        )

        ret_of_qry = self.stg_eng.query_specific_num_of_his_md_after_ts(
            topic="MD@SSE@Spot@204001@Trades", ts=1672297393990036, num=2
        )

        ret_of_qry = self.stg_eng.query_specific_num_of_his_md_before_ts(
            topic="MD@SSE@Spot@204001@Trades", ts=1672297393990036, num=2
        )

        ret_of_qry = self.stg_eng.query_his_md_between_2_ts(
            topic="MD@SSE@Spot@204001@Trades",
            ts_begin=1672297393990036,
            ts_end=1672297393990046,
        )

        ret_of_qry = self.stg_eng.query_specific_num_of_his_md_after_ts(
            topic="MD@SZSE@Spot@000001@Orders", ts=1672362900500001, num=2
        )

        ret_of_qry = self.stg_eng.query_specific_num_of_his_md_before_ts(
            topic="MD@SZSE@Spot@000001@Orders", ts=1672362900500003, num=2
        )

        ret_of_qry = self.stg_eng.query_his_md_between_2_ts(
            topic="MD@SZSE@Spot@000001@Orders",
            ts_begin=1672362900500001,
            ts_end=1672362900500003,
        )

        print(f"===== {ret_of_qry[0]}")
        print(f"===== {ret_of_qry[1]}")

        return

    def on_stg_inst_start(self, stg_inst_info):
        if stg_inst_info.stg_inst_id == 1:
            self.stg_eng.sub(
                stg_inst_info.stg_inst_id, "shm://MD.SSE.Spot/588180/Tickers"
            )
            self.stg_eng.sub(
                stg_inst_info.stg_inst_id, "shm://MD.SSE.Spot/603123/Orders"
            )
            self.stg_eng.sub(
                stg_inst_info.stg_inst_id, "shm://MD.SZSE.Spot/000002/Trades"
            )

            # sub pos info of stg id 10000, note that the topic is case sensitive.
            self.stg_eng.sub(
                stg_inst_info.stg_inst_id,
                "shm://RISK.PubChannel.Trade/PosInfo/StgId/10000",
            )

            # sub pos info of stg inst id 1, note that the topic is case sensitive.
            self.stg_eng.sub(
                stg_inst_info.stg_inst_id,
                "shm://RISK.PubChannel.Trade/PosInfo/StgId/10000/StgInstId/1",
            )

            # sub pos info of stg inst id 2, note that the topic is case sensitive.
            self.stg_eng.sub(
                stg_inst_info.stg_inst_id,
                "shm://RISK.PubChannel.Trade/PosInfo/StgId/10000/StgInstId/2",
            )

        if stg_inst_info.stg_inst_id == 2:
            # sub trigger risk ctrl of plugin
            self.stg_eng.sub(
                stg_inst_info.stg_inst_id,
                "shm://RISK.PlugInChannel.Trade/TriggerRiskCrtl",
            )

    def on_stg_inst_timer(self, stg_inst_info, timer_name):
        if timer_name == "TestRealOrder":
            print(f"Timer {timer_name} was triggered")
            for i in range(0, 1):
                self.__test_real_order(stg_inst_info)

        if timer_name == "TestSimedOrder":
            print(f"Timer {timer_name} was triggered")
            for i in range(0, 10000):
                self.__test_simed_order(stg_inst_info)

        if timer_name == "TestCancelOrder":
            print(f"Timer {timer_name} was triggered")
            for i in range(0, 2):
                self.stg_eng.cancel_order(155704551574893756)

        return

    def __test_real_order(self, stg_inst_info):
        ret_of_order = self.stg_eng.order(
            stg_inst_info,
            acct_id=10000,
            market_code=MarketCode.SZSE,
            symbol_code="000002",
            side=Side.Bid,
            pos_side=PosSide.Both,
            order_price=19,
            order_size=100,
        )

        status_code = ret_of_order[0]
        if status_code != 0:
            status_msg = get_status_msg(status_code)
            print(f"Create order failed. {status_code} - {status_msg}")
            return

        order_id = ret_of_order[1]

        ret_of_get_order_info = self.stg_eng.get_order_info(order_id)
        status_code = ret_of_get_order_info[0]
        if status_code != 0:
            status_msg = get_status_msg(status_code)
            print(f"Get order info failed. {status_code} - {status_msg}")
            return

        order_info = ret_of_get_order_info[1]
        print(order_info)
        return

    def __test_simed_order(self, stg_inst_info):
        return

    def __test_cancel_order(self, stg_inst_info):
        self.stg_eng.cancel_order(order_id)
        return

    def on_stg_manual_intervention(self, stg_inst_info, stg_manual_intervention):
        print(stg_manual_intervention)
        data = json.dumps(stg_manual_intervention)

    def on_push_topic(self, stg_inst_info, topic_data):
        print(f"stg inst {stg_inst_info.stg_inst_id} recv {topic_data}")

    def on_order_ret(self, stg_inst_info, order_info):
        return
        print(f"on order ret {order_info.to_short_str()}")

    def on_cancel_order_ret(self, stg_inst_info, order_info):
        return
        print(f"on cancel order ret {order_info.to_short_str()}")

    def on_trades(self, stg_inst_info, trades):
        market_data = json.dumps(trades)

    def on_books(self, stg_inst_info, books):
        market_data = json.dumps(books)

    def on_tickers(self, stg_inst_info, tickers):
        market_data = json.dumps(tickers)

    def on_candle(self, stg_inst_info, candle):
        market_data = json.dumps(candle)

    def on_stg_inst_add(self, stg_inst_info):
        # Write strategy business code here
        pass

    def on_stg_inst_del(self, stg_inst_info):
        # Write strategy business code here
        pass

    def on_stg_inst_chg(self, stg_inst_info):
        # Write strategy business code here
        pass

    def on_pos_update_of_acct_id(self, stg_inst_info, pos_snapshot):
        return

    def on_pos_snapshot_of_acct_id(self, stg_inst_info, pos_snapshot):
        return

    def on_pos_update_of_stg_id(self, stg_inst_info, pos_snapshot):
        # query pnl of stgId=10000&stgInstId=1
        if stg_inst_info.stg_inst_id == 1:
            ret_of_query_pnl = pos_snapshot.query_pnl(
                query_cond="stgId=10000&stgInstId=1",
                quote_currency_for_calc="CNY",
                quote_currency_for_conv="CNY",
            )
            status_code = ret_of_query_pnl[0]
            if status_code == 0:
                pnl = ret_of_query_pnl[1]
                print(f"pnl = {pnl.to_str()}")
            else:
                status_msg = get_status_msg(status_code)
                print(f"{status_code} - {status_msg}")
        return

    def on_pos_snapshot_of_stg_id(self, stg_inst_info, pos_snapshot):
        pass

    def on_pos_update_of_stg_inst_id(self, stg_inst_info, pos_snapshot):
        # Write strategy business code here
        pass

    def on_pos_snapshot_of_stg_inst_id(self, stg_inst_info, pos_snapshot):
        # Write strategy business code here
        pass

    def on_assets_update(self, stg_inst_info, assets_update):
        return

    def on_assets_snapshot(self, stg_inst_info, assets_snapshot):
        return


def parse_argv(argv):
    conf = ""
    try:
        opts, args = getopt.getopt(argv[1:], "hc:", ["conf="])
    except getopt.GetoptError:
        print(f"{argv[0]} -c <configfile> or {argv[0]} --conf=<configfile>")
        sys.exit(2)
    for opt, arg in opts:
        if opt == "-h":
            print(f"{argv[0]} -c <configfile> or {argv[0]} --conf=<configfile>")
            sys.exit()
        elif opt in ("-c", "--conf"):
            conf = arg
    return conf


def main(argv):
    conf = parse_argv(argv)
    if len(conf) == 0:
        print(f"{argv[0]} -c <configfile> or {argv[0]} --conf=<configfile>")
        sys.exit(3)

    stg_eng = StgEng(conf)
    stg_inst_task_handler = StgInstTaskHandler(stg_eng)
    ret = stg_eng.init(stg_inst_task_handler)
    if ret != 0:
        sys.exit(4)
    stg_eng.run()


if __name__ == "__main__":
    main(sys.argv)
