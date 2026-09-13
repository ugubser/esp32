#pragma once
#include "transit_model.h"
#include "cJSON.h"
namespace lcars {
inline bool transit_parse(const std::string &body,unsigned stop,int64_t now,TransitBoard &board) {
  const char *end=nullptr;
  cJSON *root=cJSON_ParseWithLengthOpts(body.c_str(),body.size()+1,&end,true);
  if(!cJSON_IsArray(root)){cJSON_Delete(root);return false;}
  for(size_t r=0;r<board.size();++r)if(TRANSIT_ROUTES[r].stop==stop)board[r]=TransitGroup{{},"",now};
  bool valid=true;
  cJSON *item=nullptr;
  cJSON_ArrayForEach(item,root) {
    auto *line=cJSON_GetObjectItemCaseSensitive(item,"number");
    auto *to=cJSON_GetObjectItemCaseSensitive(item,"to");
    auto *at=cJSON_GetObjectItemCaseSensitive(item,"departure");
    auto *delay=cJSON_GetObjectItemCaseSensitive(item,"delay");
    if(!cJSON_IsString(line)||!cJSON_IsString(to)||!cJSON_IsString(at)||!cJSON_IsNumber(delay)) {valid=false;break;}
    const auto timestamp=transit_timestamp(at->valuestring);
    if(!timestamp || delay->valuedouble<0 || delay->valuedouble>1440) {valid=false;break;}
    for(size_t r=0;r<board.size();++r)
      if(TRANSIT_ROUTES[r].stop==stop && transit_matches(r,line->valuestring,to->valuestring))
        transit_add(board[r],{to->valuestring,timestamp,delay->valueint},now);
  }
  cJSON_Delete(root);return valid;
}
}
