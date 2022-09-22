```text
      __         __  __                                       __ 
     / /_  ___  / /_/ /____  _____   ____ ___  ______ _____  / /_
    / __ \/ _ \/ __/ __/ _ \/ ___/  / __ `/ / / / __ `/ __ \/ __/
   / /_/ /  __/ /_/ /_/  __/ /     / /_/ / /_/ / /_/ / / / / /_  
  /_.___/\___/\__/\__/\___/_/      \__, /\__,_/\__,_/_/ /_/\__/  
                                     /_/                         
```                                                        

[![github](https://img.shields.io/badge/github-byrnexu-brightgreen.svg)](https://github.com/byrnexu)

# Better Quant    <code><img height="20" src="https://raw.githubusercontent.com/github/explore/80688e429a7d4ef2fca1e82350fe8e3517d3494d/topics/cpp/cpp.png" alt="cpp"></code>  
**Better quant today, best quant tomorrow.** 💪
  ## 前言
&emsp;&emsp;目前交易所只接了数字货币的币安，但是国内现货和其他衍生品的仓位管理、盈亏计算、资产管理的算法及订单的状态维护基本都类似，稍加修改即可同时支持国内的二级市场，作为一个独立开发者，时间和精力有限，这些功能就暂时放在todo list里了。另外如果要接入更多的交易所，基于目前良好的行情和交易网关的接口，也可以逐个快速接入。betterquant的主要功能和特点包括：<br/>
* 🔥 支持的交易品种包括现货、币本位和U本位永续合约、币本位和U本位交割合约，以及上述合约的单边持仓和双向持仓的仓位管理和盈亏监控功能。当然，前面也提到了，和国内期现货的交易规则类似，让系统支持国内现货和衍生品，除了行情和交易网关接入、因为今仓昨仓而造成的pnl和手续费算法的不同，其他方面基本上不需要做太大的修改❕。目前之所以只接了加密货币，主要目的是先把业务逻辑先打通，系统设计的初衷是同时支持传统二级市场和数字货币的现货及其他衍生品的‼️。加密货币有正反向合约有交割也有永续，有单边持仓双向持仓，有全仓和逐仓，而且是7\*24小时交易的，另外每个交易所的接口差异较大，总的来说比国内的期现业务要相对复杂，所以一套系统从支持加密货币到传统传统市场比支持传统市场的系统继而支持加密市场肯定是要容易的多，所以我先打通是加密货币这条通道。<br/>
&nbsp;
* 🔥🔥 强大的灾备功能，任何子系统崩溃，不会导致最终的数据异常‼️。交易服务崩溃，重启后会重建仓位和PNL信息，交易网关崩溃重启后会自动处理崩溃期间产生的未处理的订单状态变化，风控子系统崩溃后重启，同样会恢复各种风控指标。上述的恢复过程无需撤销任何未完结的订单。当然，作为一个专业的程序员，我已经很久没遇到生产环境中系统崩溃的问题了。<br/>
&nbsp;
* 🔥 所有子系统，包括行情子系统、策略引擎、交易服务、交易网关、风控子系统都通过无锁共享内存交互❕。成功的消灭常规方案也就是子系统间通过tcp/domainsocket的百微秒级别的延时❕。用共享内存做子系统之间的ipc，使得系统兼具单进程的性能，同时也具备多进程的安全性，即任意系统crash不会导致其他子系统crash。当然虽然 目前子系统之间是通过共享内存交互的，但这并不是一个单机版的交易系统❕，后续会架设web服务，提供restful和websocket接口，接受远端报单、回报和其他业务功能，同时也将多台托管服务器联系起来。<br/>
&nbsp;
* 🔥 每个子系统有自己独立的PUB_CHANNEL，你可以往自己的PUB_CHANNEL发布TOPIC，比如行情子系统可以发布新合约上线、合约参数的变化情况、风控子系统可以定制自己的风控指标，发布到风控子系统的PUB_CHANNEL，每个子系统可以通过系统的统一的格式订阅和发布自己感兴趣的TOPIC‼️。<br/>
&nbsp;
* 🔥 账户、策略、子策略、用户等各个层面独立的仓位和订单管理功能。<br/>
&nbsp;
* 🔥 很多系统收到报单后会针对订单做事前风控的处理，这个环节成为了这些系统的瓶颈所在，系统默认对收到的订单按照账户维度做了分流，实现了无锁并行的风控处理，使得这个环节变得畅通无阻✈️，当然也可以对代码稍加修改或者以后增加配置项，根据配置指定参数实现更细粒度维度的请求分流。<br/>
&nbsp;
* 🔥🔥 插件式的风控指标管理🔌，可以根据系统统一的格式撰写动态链接库形式的风控指标，在交易系统运行中你可以启用、禁用或者升级这些风控插件‼️，从而实现风控指标的动态管理，满足7\*24小时交易需要，开放式的api接口使得你可以在风控指标接口里得到你任何想要的数据，通过这些数据结合自己的需求定制灵活多样化的风控指标。<br/>
&nbsp;
* 🔥🔥 强大的pnl实时监控功能‼️，你可以在任何子系统（比如策略端）实时订阅并监控账户、策略、子策略、用户、市场、标的、多头头寸、空头头寸等每个维度的已实现盈亏、未实现盈亏（浮动盈亏）、手续费使用情况，或者说任意维度组合的以任意币种计价的已实现盈亏、未实现盈亏（浮动盈亏）、手续费使用情况‼️，例如：</br>  
&emsp;&emsp;🌶️ 订阅监控策略编号10000在币安交易币本位ETH永续合约以BTC计价的pnl和手续费情况：
```c++
    // sub
    getStgEng()->sub(stgInstInfo->stgInstId_,
                     "shm://RISK.PubChannel.Trade/PosInfo/StgId/10000");
    // on callback                     
    const auto queryCond = "stgId=10000&marketCode=Binance&symbolCode=ETH-USD-CPerp";
    const auto [statusCode, pnl] =
        posSnapshot->queryPnl(queryCond, getStgEng()->getMarketDataCache(),
                              QuoteCurrencyForCalc("BTC"));
```
&emsp;&emsp;&emsp;&emsp;🌶️ 订阅监控账户10000在币安交易现货币对ETH-BTC，以BUSD计价的的pnl和手续费情况：
```c++
    // sub
    getStgEng()->sub(stgInstInfo->stgInstId_,
                     "shm://RISK.PubChannel.Trade/PosInfo/AcctId/10000");
    // on callback                     
    const auto queryCond = "stgId=10000&marketCode=Binance&symbolCode=ETH-BTC";
    const auto [statusCode, pnl] =
        posSnapshot->queryPnl(queryCond, getStgEng()->getMarketDataCache(),
                              QuoteCurrencyForCalc("BUSD"));
```

&emsp;&emsp;你也可以同时监控多个策略组合，计算策略组合的已实现盈亏、未实现盈亏和手续费的使用情况，通过这些盈亏数据，你甚至可以撰写一个专门监控其他策略的监控策略❕，触发风控后往其他策略发送干预指令等种种灵活的功能。相信很多资深的架构师都会有同感，**当系统架构设计趋于科学合理，有时候在实现过程中会冒出很多并不在你最初的考虑中但又很实用的功能** ‼️。当然以后增加算法交易功能之后，你也可以 实时跟踪某一个算法单（包含多个子单）的盈亏情况。<br/>
&nbsp;
* 🔥 系统外状态码动态维护功能，由于交易系统可能会接入多个交易所/交易柜台，每个外部交易服务都有自己特定的状态码，策略执行过程中也会收到未知的外部状态，系统在收到这些外部状态之后会自动收录，你可以在运行期间将其映射到指定的内部状态码，由此策略端就能够正确的处理新的状态码的业务逻辑。整个过程无需重启任何子系统❕。<br/>
&nbsp;
* 🔥 交易代码表动态维护功能，系统为定时检测交易所代码表变动情况，除了新代码上下线还有他的一些最大最小的交易以及金额的变动单位等等，如果有变化立刻更新本地码表。<br/>
&nbsp;
* 🔥 每一个策略可以指定若干套参数的子策略，子策略集合由一个线程池管理，子策略的参数可以在运行期间修改并自动更新，也可以在运行中增加或者禁用子策略❕。这一点对于一些7\*24小时交易的市场尤为重要。<br/>
&nbsp;
* 🔥 目前系统内延迟是10微秒这个量级（阿里云 Intel(R)cpu Xeon(R) Platinum 8369B CPU @ 2.70GHz），也就是行情进入系统开始计时、dispatch到convert线程转换为内部统一格式、发布到行情子系统的PUB_CHANNEL，策略收到行情将行情dispatch到订阅该行情的子策略，子策略发起报单，交易服务收到报单，经过最简单的风控（目前内置了流控插件），订单由交易服务发往交易网关，交易网关将订单转换为交易所的格式后发出后计时结束。后续版本考虑将子系统编译成库，使得整套系统既可以部署为多进程系统（具备多进程的安全性，策略或者其他子系统崩溃不会导致交易和风控等服务崩溃），也可以部署为单进程（实现纳秒级系统内延时）。<br/>
---
## 目录
### 🛠 [编译](doc/build.md)
### 🐋 [安装](doc/installation.md)
### ⭐ [文档](doc/documentation.md)
### 🧨 [注意](doc/caution.md)
### ⁉️ [FAQ](doc/faq.md)
### 🥔 [TODO](doc/todo.md)

---
<div align="center"> <img  src="https://github-readme-streak-stats.herokuapp.com?user=byrnexu&theme=onedark&date_format=M%20j%5B%2C%20Y%5D" /> </div>

