    template <typename Visitor>
    static bool VisitRangeReverse(const PieceTreeNode* node,size_t first,size_t last,Visitor&& visitor)
    {
        if(first==last)return true;
        for(auto next=node->FindEntry(last-1u)+1u;next>0u;)
        {
            const auto index=--next;
            const auto begin=node->EntryStart(index),end=node->EntryEnd(index);
            const auto left=std::max(first,begin),right=std::min(last,end);
            if(node->height)
            {
                if(!VisitRangeReverse(std::get<Branches>(node->entries)[index].node.get(),left-begin,right-begin,visitor))return false;
            }
            else
            {
                const auto& piece=std::get<Leaves>(node->entries)[index];
                if(!visitor(std::wstring_view(*piece.buffer).substr(piece.bufferStart+left-begin,right-left)))return false;
            }
            if(begin<=first)break;
        }
        return true;
    }
