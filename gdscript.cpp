//+------------------------------------------------------------------+
//|                                     PMax_Consolidation_EA.mq5    |
//|                                  Copyright 2026, Auto-Generated  |
//|                                             https://www.mql5.com |
//+------------------------------------------------------------------+
#property copyright "Copyright 2026"
#property link      "https://www.mql5.com"
#property version   "1.00"

// --- BIBLIOTECAS DE NEGOCIAÇÃO ---
#include <Trade\Trade.mqh>
CTrade trade;

// --- INPUTS (PARÂMETROS DE ENTRADA) ---
input group "=== PMax Settings ==="
input int      InpAtrPeriod     = 10;      // ATR Length
input double   InpAtrMultiplier = 1.5;     // ATR Multiplier
input int      InpMaLength      = 12;      // MA Length

input group "=== Filtro de Consolidação ==="
input bool     InpUseConsolidation = true; // Ativar Filtro de Consolidação
input int      InpLookback      = 20;      // Janela de Barras (Lookback)
input int      InpMaxCrosses    = 3;       // Máximo de Cruzamentos Permitidos

input group "=== Gestão de Risco (%) ==="
input double   InpStopLossPct   = 1.5;     // Stop Loss (%)
input double   InpTakeProfitPct = 1.5;     // Take Profit 1 (%)
input double   InpLotSize       = 0.1;     // Lote Fixo

// --- VARIÁVEIS GLOBAIS DE HANDLES ---
int atrHandle;
int maHandle;
int ema9Handle;
int ema21Handle;
int macdHandle;
datetime lastBarTime;
int pmaxDir = 1;

//+------------------------------------------------------------------+
//| Expert initialization function                                   |
//+------------------------------------------------------------------+
int OnInit()
  {
   // Inicializa os handles dos indicadores nativos do MT5
   atrHandle   = iATR(_Symbol, _Period, InpAtrPeriod);
   maHandle    = iMA(_Symbol, _Period, InpMaLength, 0, MODE_EMA, PRICE_CLOSE);
   ema9Handle  = iMA(_Symbol, _Period, 9, 0, MODE_EMA, PRICE_CLOSE);
   ema21Handle = iMA(_Symbol, _Period, 21, 0, MODE_EMA, PRICE_CLOSE);
   macdHandle  = iMACD(_Symbol, _Period, 12, 26, 9, PRICE_CLOSE);

   if(atrHandle == INVALID_HANDLE || maHandle == INVALID_HANDLE || 
      ema9Handle == INVALID_HANDLE || ema21Handle == INVALID_HANDLE || macdHandle == INVALID_HANDLE)
     {
      Print("Erro ao criar handles dos indicadores.");
      return(INIT_FAILED);
     }

   lastBarTime = 0;
   return(INIT_SUCCEEDED);
}

//+------------------------------------------------------------------+
//| Expert deinit function                                           |
//+------------------------------------------------------------------+
void OnDeinit(const int reason)
  {
   // Libera os handles da memória
   IndicatorRelease(atrHandle);
   IndicatorRelease(maHandle);
   IndicatorRelease(ema9Handle);
   IndicatorRelease(ema21Handle);
   IndicatorRelease(macdHandle);
  }

//+------------------------------------------------------------------+
//| Expert tick function                                             |
//+------------------------------------------------------------------+
void OnTick()
  {
   // Verifica se é uma nova barra para rodar a lógica apenas no fechamento anterior
   datetime currentBarTime = iTime(_Symbol, _Period, 0);
   if(currentBarTime == lastBarTime) return;
   
   // Obtém arrays de preços históricos (barra 1 e barra 2)
   MqlRates rates[];
   ArraySetAsSeries(rates, true);
   if(CopyRates(_Symbol, _Period, 0, 3, rates) < 3) return;

   // --- 1. CAPTURA DE VALORES DOS INDICADORES ---
   double atr[], ma[], ema9[], ema21[], macdMain[], macdSignal[];
   ArraySetAsSeries(atr, true);
   ArraySetAsSeries(ma, true);
   ArraySetAsSeries(ema9, true);
   ArraySetAsSeries(ema21, true);
   ArraySetAsSeries(macdMain, true);
   ArraySetAsSeries(macdSignal, true);

   if(CopyBuffer(atrHandle, 0, 1, 1, atr) <= 0) return;
   if(CopyBuffer(maHandle, 0, 1, 1, ma) <= 0) return;
   if(CopyBuffer(ema9Handle, 0, 0, 2, ema9) <= 0) return;   // [0] atual, [1] anterior
   if(CopyBuffer(ema21Handle, 0, 0, 2, ema21) <= 0) return; // [0] atual, [1] anterior
   if(CopyBuffer(macdHandle, 0, 1, 1, macdMain) <= 0) return;
   if(CopyBuffer(macdHandle, 1, 1, 1, macdSignal) <= 0) return;

   // --- 2. CÁLCULO DO PMAX ---
   double longStop = ma[0] - (InpAtrMultiplier * atr[0]);
   double shortStop = ma[0] + (InpAtrMultiplier * atr[0]);

   if(pmaxDir == 1 && rates[1].close < longStop)
      pmaxDir = -1;
   else if(pmaxDir == -1 && rates[1].close > shortStop)
      pmaxDir = 1;

   double pmaxVal = (pmaxDir == 1) ? longStop : shortStop;

   // --- 3. FILTRO DE CONSOLIDAÇÃO (Varre o histórico recente do Lookback) ---
   int totalCrosses = 0;
   if(InpUseConsolidation)
     {
      double e9[], e21[];
      ArraySetAsSeries(e9, true);
      ArraySetAsSeries(e21, true);
      
      // Copia o histórico de cruzamentos da EMA 9 e EMA 21
      if(CopyBuffer(ema9Handle, 0, 1, InpLookback + 1, e9) > 0 &&
         CopyBuffer(ema21Handle, 0, 1, InpLookback + 1, e21) > 0)
        {
         for(int i = 0; i < InpLookback; i++)
           {
            bool crossed = (e9[i+1] <= e21[i+1] && e9[i] > e21[i]) || 
                           (e9[i+1] >= e21[i+1] && e9[i] < e21[i]);
            if(crossed) totalCrosses++;
           }
        }
     }

   bool isConsolidated = InpUseConsolidation && (totalCrosses >= InpMaxCrosses);
   bool notConsolidated = !isConsolidated;

   // --- 4. CONDIÇÕES DE ENTRADA E SAÍDA ---
   bool macdCond = (macdMain[0] > macdSignal[0]);
   bool buyCondition = macdCond && (rates[1].close > pmaxVal) && notConsolidated;
   bool sellCondition = (rates[1].close < pmaxVal) || (macdMain[0] < macdSignal[0]);

   // --- 5. EXECUÇÃO DAS ORDENS ---
   if(PositionsTotal() == 0)
     {
      if(buyCondition)
        {
         double ask = SymbolInfoDouble(_Symbol, SYMBOL_ASK);
         double sl = ask * (1.0 - (InpStopLossPct / 100.0));
         double tp = ask * (1.0 + (InpTakeProfitPct / 100.0));

         trade.Buy(InpLotSize, _Symbol, ask, sl, tp, "PMax Buy");
         lastBarTime = currentBarTime;
        }
     }
   else
     {
      // Se já houver posição aberta, verifica a saída
      if(PositionSelect(_Symbol))
        {
         if(PositionGetInteger(POSITION_TYPE) == POSITION_TYPE_BUY && sellCondition)
           {
            trade.PositionClose(PositionGetTicket());
           }
        }
     }
  }
//+------------------------------------------------------------------+
