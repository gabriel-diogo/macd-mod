//+------------------------------------------------------------------+
//|                                        PMax_MACD_Strategy.mq5    |
//|                                  Copyright 2026, Personal Bot    |
//|                                             https://www.mql5.com |
//+------------------------------------------------------------------+
#property copyright "Copyright 2026"
#property link      "https://www.mql5.com"
#property version   "1.00"

#include <Trade\Trade.mqh>
CTrade trade;

//--- Parâmetros de Entrada (Inputs)
input group "--- Gestão de Risco ---"
input double InpLotSize       = 1.0;          // Tamanho do Lote
input double InpStopLossPct   = 1.5;          // Stop Loss Inicial (%)
input double InpTakeProfitPct = 4.5;          // Take Profit (%) [Alvo 1:3]

input group "--- Parâmetros de Médias e MACD ---"
input int    InpFastLen       = 12;           // MACD Fast Length
input int    InpSlowLen       = 26;           // MACD Slow Length
input int    InpSignalLen     = 9;            // MACD Signal Smoothing

//--- Variáveis de Handles dos Indicadores
int macdHandle;
int ema9Handle;
int ema21Handle;
int ema50Handle;

//+------------------------------------------------------------------+
//| Expert initialization function                                   |
//+------------------------------------------------------------------+
int OnInit()
{
   // Inicializando os handles dos indicadores técnicos
   macdHandle = iMACD(_Symbol, _Period, InpFastLen, InpSlowLen, InpSignalLen, PRICE_CLOSE);
   ema9Handle  = iMA(_Symbol, _Period, 9, 0, MODE_EMA, PRICE_CLOSE);
   ema21Handle = iMA(_Symbol, _Period, 21, 0, MODE_EMA, PRICE_CLOSE);
   ema50Handle = iMA(_Symbol, _Period, 50, 0, MODE_EMA, PRICE_CLOSE);

   // Verificando se os indicadores foram carregados com sucesso
   if(macdHandle == INVALID_HANDLE || ema9Handle == INVALID_HANDLE || 
      ema21Handle == INVALID_HANDLE || ema50Handle == INVALID_HANDLE)
   {
      Print("Erro ao inicializar os indicadores técnicos.");
      return(INIT_FAILED);
   }

   Print("Expert Advisor inicializado com sucesso.");
   return(INIT_SUCCEEDED);
}

//+------------------------------------------------------------------+
//| Expert deinitialization function                                 |
//+------------------------------------------------------------------+
void OnDeinit(const int reason)
{
   // Liberando os handles ao fechar o EA
   IndicatorRelease(macdHandle);
   IndicatorRelease(ema9Handle);
   IndicatorRelease(ema21Handle);
   IndicatorRelease(ema50Handle);
}

//+------------------------------------------------------------------+
//| Expert tick function                                             |
//+------------------------------------------------------------------+
void OnTick()
{
   // Controle para rodar a lógica apenas no fechamento de uma nova barra
   static datetime last_bar_time = 0;
   datetime current_bar_time = iTime(_Symbol, _Period, 0);
   if(current_bar_time == last_bar_time) return;
   
   // Arrays para armazenar os valores dos buffers dos indicadores
   double macdMain[], macdSignal[];
   double ema9[], ema21[], ema50[];
   
   ArraySetAsSeries(macdMain, true);
   ArraySetAsSeries(macdSignal, true);
   ArraySetAsSeries(ema9, true);
   ArraySetAsSeries(ema21, true);
   ArraySetAsSeries(ema50, true);
   
   // Copiando os dados dos indicadores para os arrays (índice 1 = barra anterior fechada)
   if(CopyBuffer(macdHandle, 0, 0, 3, macdMain) <= 0) return;
   if(CopyBuffer(macdHandle, 1, 0, 3, macdSignal) <= 0) return;
   if(CopyBuffer(ema9Handle, 0, 0, 3, ema9) <= 0) return;
   if(CopyBuffer(ema21Handle, 0, 0, 3, ema21) <= 0) return;
   if(CopyBuffer(ema50Handle, 0, 0, 3, ema50) <= 0) return;

   // Lógica de Cruzamento: EMA 9 cruza para cima a EMA 21
   bool ema_cross_up = (ema9[1] > ema21[1] && ema9[2] <= ema21[2]);
   
   // Confirmação MACD: Linha MACD acima da linha de Sinal e > 0
   bool macd_ok = (macdMain[1] > macdSignal[1] && macdMain[1] > 0);
   
   // Filtro de Tendência: Preço de fechamento acima da EMA 50
   MqlRates rates[];
   ArraySetAsSeries(rates, true);
   if(CopyRates(_Symbol, _Period, 0, 3, rates) <= 0) return;
   bool trend_ok = (rates[1].close > ema50[1]);

   // Gatilho de Compra Final
   bool buy_signal = ema_cross_up && macd_ok && trend_ok;

   // Execução da Ordem se não houver posições abertas
   if(PositionsTotal() == 0 && buy_signal)
   {
      double ask = SymbolInfoDouble(_Symbol, SYMBOL_ASK);
      double sl = ask * (1.0 - InpStopLossPct / 100.0);
      double tp = ask * (1.0 + InpTakeProfitPct / 100.0);
      
      // Enviando ordem de compra
      if(trade.Buy(InpLotSize, _Symbol, ask, sl, tp, "EA_PMax_MACD"))
      {
         last_bar_time = current_bar_time;
         Print("Ordem de Compra executada com sucesso a: ", ask);
      }
      else
      {
         Print("Erro ao executar ordem. Código de erro: ", trade.ResultRetcode());
      }
   }
}
//+------------------------------------------------------------------+
