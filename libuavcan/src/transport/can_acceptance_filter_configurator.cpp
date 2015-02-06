#include <uavcan/transport/can_acceptance_filter_configurator.hpp>
//#include <uavcan/debug.hpp>
#include <cassert>


namespace uavcan
{

void CanAcceptanceFilterConfigurator::computeConfiguration()
{
    fillArray();

    uint16_t i_ind, j_ind, rank, best_rank, i_rank = 0, j_rank = 0;
    FilterConfig temp_array;

    while (getNumFilters()  >= configs_.size())      /// "=" because we need +1 iteration for ServiceResponse
    {
        best_rank = 0;
        for (i_ind = 0; i_ind < configs_.size() - 1; i_ind++)
        {
            for (j_ind = 1; j_ind < configs_.size(); j_ind++)
            {
                if (((configs_[IndexType(i_ind)].id & configs_[IndexType(i_ind)].mask) | (configs_[IndexType(j_ind)].id & configs_[IndexType(j_ind)].mask)) != 0)
                {
                    temp_array = mergeFilters(configs_[IndexType(i_ind)], configs_[IndexType(j_ind)]);
                    rank = static_cast<uint16_t>(countBits(temp_array.mask));                        // CHECK static_cast<uint16_t>

                    if (rank > best_rank)
                    {
                        best_rank = rank;
                        i_rank = i_ind;
                        j_rank = j_ind;
                    }
                }
            }
        }

        configs_[IndexType(j_rank)] = mergeFilters(configs_[IndexType(i_rank)], configs_[IndexType(j_rank)]);
        configs_[IndexType(i_rank)].id = configs_[IndexType(i_rank)].mask = 0;
    }

    cleanZeroItems();

    FilterConfig ServiseRespFrame;
    ServiseRespFrame.id = 0;
    ServiseRespFrame.mask = 0x00060000;
    configs_.push_back(ServiseRespFrame);
}

void CanAcceptanceFilterConfigurator::fillArray()
{
    const TransferListenerBase* p = node_.getDispatcher().getListOfMessageListeners().get();
    configs_.clear();

    while (p)
    {
        FilterConfig cfg;

        cfg.id = static_cast<uint32_t>(p->getDataTypeDescriptor().getID().get()) << 19;
        cfg.id |= static_cast<uint32_t>(p->getDataTypeDescriptor().getKind()) << 17;
        cfg.mask = DefaultFilterMask;

        configs_.push_back(cfg);
        p = p->getNextListNode();
    }
}

void CanAcceptanceFilterConfigurator::cleanZeroItems()
{
    FilterConfig switch_array;

    for (int i = 0; i < configs_.size(); i++)
    {
        while ((configs_[IndexType(i)].mask & configs_[IndexType(i)].id) == 0)
        {
            switch_array = configs_[configs_.size()];
            configs_[IndexType(i)] = configs_[IndexType(configs_.size())]; // TO-DO check is it right - configs_[IndexType(configs_.size())] ??
            configs_[IndexType(configs_.size())] = switch_array;
            if ((i == configs_.size()) & ((configs_[IndexType(i)].mask & configs_[IndexType(i)].id) == 0)) // this function precludes two
            {                                                                        // last elements zeros
                configs_.pop_back();
                break;
            }
            configs_.pop_back();
        }
    }
}

CanAcceptanceFilterConfigurator::FilterConfig CanAcceptanceFilterConfigurator::mergeFilters(FilterConfig &a_, FilterConfig &b_)
{
    FilterConfig temp_arr;
    temp_arr.mask = a_.mask & b_.mask & ~(a_.id ^ b_.id);
    temp_arr.id = a_.id & temp_arr.mask;

    return (temp_arr);
}

uint32_t CanAcceptanceFilterConfigurator::countBits(uint32_t n_)
{
    uint32_t c_; // c accumulates the total bits set in v
    for (c_ = 0; n_; c_++)
    {
        n_ &= n_ - 1; // clear the least significant bit set
    }
    return c_;
}

uint16_t CanAcceptanceFilterConfigurator::getNumFilters() const
{
    static const uint16_t InvalidOut = 0xFFFF;
    uint16_t out = InvalidOut;
    // TODO HACK FIXME make getCanDriver() return a mutable reference
    ICanDriver& can_driver = const_cast<ICanDriver&>(node_.getDispatcher().getCanIOManager().getCanDriver());

    for (uint8_t i = 0; i < node_.getDispatcher().getCanIOManager().getNumIfaces(); i++)
    {
        const ICanIface* iface = can_driver.getIface(i);
        if (iface == NULL)
        {
            UAVCAN_ASSERT(0);
            out = 0;
            break;
        }
        const uint16_t num = iface->getNumFilters();
        out = min(out, num);
    }

    return (out == InvalidOut) ? 0 : out;
}

}


