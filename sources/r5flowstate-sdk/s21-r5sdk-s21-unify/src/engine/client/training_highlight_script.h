#pragma once
static const char* const kTrainingHighlightScript=R"sq(
thread (void function(){
 array<entity> actors;
 array<int> contexts;
 array<vector> colors,secondaryColors;
 array<bool> insideOn,outlineOn;
 bool configured=false;
 float nextScan=0;
 void functionref() cleanup=void function():(actors,contexts,colors,secondaryColors,insideOn,outlineOn){
  foreach(int i,entity actor in actors){
   if(!IsValid(actor))continue;
   actor.Highlight_SetCurrentContext(contexts[i]);
   actor.Highlight_OverrideParam(0,colors[i]);actor.Highlight_OverrideParam(1,secondaryColors[i]);
   if(insideOn[i])actor.Highlight_ShowInside(0.0);else actor.Highlight_HideInside(0.0);
   if(outlineOn[i])actor.Highlight_ShowOutline(0.0);else actor.Highlight_HideOutline(0.0);
   ManageHighlightEntity(actor);
  }
  actors.clear();contexts.clear();colors.clear();secondaryColors.clear();insideOn.clear();outlineOn.clear();
  TrainingHighlight_Restore();
 };
 OnThreadEnd(cleanup);
 while(IsValid(GetLocalClientPlayer())){
  if(!GetConVarBool("local_model_highlight")||!TrainingAim_SessionReady()){
   if(configured){cleanup();configured=false;}
   WaitFrame();continue;
  }
  int context=HighlightContext_GetId("bloodhound_sonar");
  if(!configured){
   configured=TrainingHighlight_Configure(context);
   if(!configured){wait 0.25;continue;}
   HighlightContext_SetRadius(context,2.0);
  }
  if(Time()>=nextScan){
   nextScan=Time()+0.5;
   for(int i=actors.len()-1;i>=0;i--){
    if(IsValid(actors[i]))continue;
    actors.remove(i);contexts.remove(i);colors.remove(i);secondaryColors.remove(i);insideOn.remove(i);outlineOn.remove(i);
   }
   array<entity> targets=GetNPCArray();targets.extend(GetPlayerArray());
   foreach(entity actor in targets){
    if(!IsValid(actor)||actor==GetLocalClientPlayer()||actors.contains(actor)||actors.len()>=128)continue;
    if(actor.GetHealth()<=0||actor.GetScriptName()==DECOY_SCRIPTNAME||actor.GetScriptName()==CONTROLLED_DECOY_SCRIPTNAME)continue;
    int previous=actor.Highlight_GetCurrentContext();contexts.append(previous);
    colors.append(TrainingHighlight_OriginalParam(previous,0));secondaryColors.append(TrainingHighlight_OriginalParam(previous,1));
    insideOn.append(actor.Highlight_GetCurrentInsideOpacity()>0.01);outlineOn.append(actor.Highlight_GetCurrentOutlineOpacity()>0.01);
    actors.append(actor);
   }
  }
  foreach(entity actor in actors){
   if(!IsValid(actor))continue;
   actor.Highlight_SetCurrentContext(context);
   actor.Highlight_OverrideParam(0,<0,1,0>);actor.Highlight_OverrideParam(1,<1,0,0>);
   actor.Highlight_SetVisibilityType(HIGHLIGHT_VIS_ALWAYS);
   actor.Highlight_SetFadeInTime(0.0);actor.Highlight_SetFadeOutTime(0.0);
   actor.Highlight_Enable();actor.Highlight_StartOn();
   actor.Highlight_ShowInside(0.0);actor.Highlight_ShowOutline(0.0);
  }
  WaitFrame();
 }
})()
)sq";
