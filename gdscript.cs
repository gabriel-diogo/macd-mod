#region Using declarations
using System;
using System.Collections.Generic;
using System.ComponentModel;
using System.ComponentModel.DataAnnotations;
using System.Windows.Media;
using NinjaTrader.Cbi;
using NinjaTrader.NinjaScript;
using NinjaTrader.NinjaScript.Strategies;
using NinjaTrader.Core.FloatingPoint;
using NinjaTrader.NinjaScript.Indicators;
#endregion

namespace NinjaTrader.NinjaScript.Strategies
{
    public class PMax_Stable_Consolidation_C# : Strategy
    {
        // --- INPUTS: PMAX & MAs ---
        [NinjaScriptProperty, Range(1, int.MaxValue), Display(Name="ATR Length", Order=1, GroupName="PMax & MAs")]
        public int Periods { get; set; }

        [NinjaScriptProperty, Range(0.1, double.MaxValue), Display(Name="ATR Multiplier", Order=2, GroupName="PMax & MAs")]
        public double Multiplier { get; set; }

        [NinjaScriptProperty, Range(1, int.MaxValue), Display(Name="MA Length", Order=3, GroupName="PMax & MAs")]
        public int MaLength { get; set; }

        // --- INPUTS: FILTRO DE CONSOLIDAÇÃO ---
        [NinjaScriptProperty, Display(Name="Ativar Filtro de Consolidação", Order=1, GroupName="Filtro de Consolidação")]
        public bool UseConsolidationFilter { get; set; }

        [NinjaScriptProperty, Range(1, 100), Display(Name="Janela de Barras (Lookback)", Order=2, GroupName="Filtro de Consolidação")]
        public int ConsolidationLookback { get; set; }

        [NinjaScriptProperty, Range(1, 20), Display(Name="Máximo de Cruzamentos Permitidos", Order=3, GroupName="Filtro de Consolidação")]
        public int MaxAllowedCrosses { get; set; }

        // --- INPUTS: GESTÃO DE RISCO ---
        [NinjaScriptProperty, Range(0.1, double.MaxValue), Display(Name="Stop Loss (%)", Order=1, GroupName="Gestão de Risco")]
        public double SlPct { get; set; }

        [NinjaScriptProperty, Range(0.1, double.MaxValue), Display(Name="Take Profit 1 (%)", Order=2, GroupName="Gestão de Risco")]
        public double Tp1Pct { get; set; }

        // Variáveis internas
        private int pmaxDir;
        private List<int> crossHistory; // Histórico de cruzamentos recentes para simular o array do Pine Script

        protected override void OnStateChange()
        {
            if (State == State.SetDefaults)
            {
                Description = "Estratégia PMax com Filtro de Consolidação em C#";
                Name = "PMax_Stable_Consolidation_C#";
                Calculate = Calculate.OnBarClose;
                EntriesPerDirection = 1;
                EntryHandling = EntryHandling.AllExceptEntries;
                IsExitOnSessionCloseStrategy = true;

                // Valores padrão
                Periods = 10;
                Multiplier = 1.5;
                MaLength = 12;
                UseConsolidationFilter = true;
                ConsolidationLookback = 20;
                MaxAllowedCrosses = 3;
                SlPct = 1.5;
                Tp1Pct = 1.5;
            }
            else if (State == State.Configure)
            {
                pmaxDir = 1;
                crossHistory = new List<int>();
            }
        }

        protected override void OnBarUpdate()
        {
            // Garantir histórico suficiente (precisamos de pelo menos a janela de lookback + 50 barras)
            int maxRequired = Math.Max(Math.Max(Periods, MaLength), ConsolidationLookback + 10);
            if (CurrentBar < maxRequired)
                return;

            // --- 1. CÁLCULOS TÉCNICOS BÁSICOS ---
            double src = (High[0] + Low[0]) / 2.0;
            double atrVal = ATR(Periods)[0];
            double mavg = EMA(MaLength)[0];

            // PMax Logic
            double longStop = mavg - (Multiplier * atrVal);
            double shortStop = mavg + (Multiplier * atrVal);
            
            if (pmaxDir == 1 && Close[0] < longStop)
                pmaxDir = -1;
            else if (pmaxDir == -1 && Close[0] > shortStop)
                pmaxDir = 1;

            double pmaxVal = (pmaxDir == 1) ? longStop : shortStop;

            // EMAs para o Filtro de Consolidação (EMA 9 e EMA 21)
            double ema9 = EMA(9)[0];
            double ema21 = EMA(21)[0];
            double ema9Prev = EMA(9)[1];
            double ema21Prev = EMA(21)[1];

            // --- 2. LÓGICA DO FILTRO DE CONSOLIDAÇÃO ---
            // Detecta se houve cruzamento da EMA 9 com a EMA 21 nesta barra exata
            bool emaCrossedNow = (ema9Prev <= ema21Prev && ema9 > ema21) || (ema9Prev >= ema21Prev && ema9 < ema21);
            
            crossHistory.Add(emaCrossedNow ? 1 : 0);
            
            // Mantém apenas o tamanho da janela de lookback na memória
            if (crossHistory.Count > ConsolidationLookback)
            {
                crossHistory.RemoveAt(0);
            }

            // Soma quantos cruzamentos aconteceram dentro da janela recente
            int totalCrossesRecent = 0;
            foreach (int val in crossHistory)
            {
                totalCrossesRecent += val;
            }

            // Se o número de cruzamentos atingir ou passar do limite, o mercado está consolidado/lateral
            bool isConsolidated = UseConsolidationFilter && (totalCrossesRecent >= MaxAllowedCrosses);
            bool notConsolidated = !isConsolidated;

            // --- 3. CONDIÇÕES DE ENTRADA E SAÍDA ---
            double macdValue = EMA(12)[0] - EMA(26)[0];
            double macdSignal = SMA(EMA(12) - EMA(26), 9)[0];
            bool macdCond = (macdValue > macdSignal);

            // A compra só acontece se o mercado NÃO estiver consolidado (além das outras regras de tendência)
            bool buyCondition = macdCond && (Close[0] > pmaxVal) && notConsolidated;

            // A venda / saída ocorre se perder o PMax ou se o MACD virar para baixo
            bool sellCondition = (Close[0] < pmaxVal) || (macdValue < macdSignal);

            // --- 4. EXECUÇÃO DAS ORDENS ---
            if (Position.MarketPosition == MarketPosition.Flat)
            {
                if (buyCondition)
                {
                    EnterLong("Buy");

                    double entryPrice = Close[0];
                    double stopLossPrice = entryPrice * (1.0 - (SlPct / 100.0));
                    double takeProfitPrice = entryPrice * (1.0 + (Tp1Pct / 100.0));

                    ExitLongStopMarket(0, true, Position.Quantity, stopLossPrice, "StopLoss", "Buy");
                    ExitLongLimit(0, true, Position.Quantity, takeProfitPrice, "TakeProfit", "Buy");
                }
            }
            else if (Position.MarketPosition == MarketPosition.Long)
            {
                if (sellCondition)
                {
                    ExitLong("Exit", "Buy");
                }
            }
        }
    }
}
