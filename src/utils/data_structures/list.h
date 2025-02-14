#ifndef NIXL_LIST_H
#define NIXL_LIST_H

class nixlLinkElem {
private:
    nixlLinkElem *next;
public:
    nixlLinkElem() 
    {
        next = NULL;
    }

    ~nixlLinkElem() {
        nixlLinkElem();
    }

    void insert(nixlLinkElem *elem)
    {
        elem->next = next;
        next = elem;
    }

    nixlLinkElem *next(nixlLinkElem *head)
    {
        return next;
    }

    nixlLinkElem *unlink()
    {
        nixlLinkElem *ret = next;
        if (NULL == ret) {
            return ret;
        }

        next = ret->next;
        return ret;
    }

} ;


#endif