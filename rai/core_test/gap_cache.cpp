#include <gtest/gtest.h>
#include <rai/node/testing.hpp>

TEST (gap_cache, add_new)
{
    rai::system system (24000, 1);
    rai::gap_cache cache (*system.nodes [0]);
    rai::send_block block1 (0, 1, 2, rai::keypair ().prv, 4, 5);
    cache.add (rai::send_block (block1), block1.previous ());
    ASSERT_NE (cache.blocks.end (), cache.blocks.find (block1.previous ()));
}

TEST (gap_cache, add_existing)
{
    rai::system system (24000, 1);
    rai::gap_cache cache (*system.nodes [0]);
    rai::send_block block1 (0, 1, 2, rai::keypair ().prv, 4, 5);
    auto previous (block1.previous ());
    cache.add (block1, previous);
    auto existing1 (cache.blocks.find (previous));
    ASSERT_NE (cache.blocks.end (), existing1);
    auto arrival (existing1->arrival);
    while (arrival == std::chrono::system_clock::now ());
    cache.add (block1, previous);
    ASSERT_EQ (1, cache.blocks.size ());
    auto existing2 (cache.blocks.find (previous));
    ASSERT_NE (cache.blocks.end (), existing2);
    ASSERT_GT (existing2->arrival, arrival);
}

TEST (gap_cache, comparison)
{
    rai::system system (24000, 1);
    rai::gap_cache cache (*system.nodes [0]);
    rai::send_block block1 (1, 0, 2, rai::keypair ().prv, 4, 5);
    auto previous1 (block1.previous ());
    cache.add (rai::send_block (block1), previous1);
    auto existing1 (cache.blocks.find (previous1));
    ASSERT_NE (cache.blocks.end (), existing1);
    auto arrival (existing1->arrival);
    while (std::chrono::system_clock::now () == arrival);
    rai::send_block block3 (0, 42, 1, rai::keypair ().prv, 3, 4);
    auto previous2 (block3.previous ());
    cache.add (rai::send_block (block3), previous2);
    ASSERT_EQ (2, cache.blocks.size ());
    auto existing2 (cache.blocks.find (previous2));
    ASSERT_NE (cache.blocks.end (), existing2);
    ASSERT_GT (existing2->arrival, arrival);
    ASSERT_EQ (arrival, cache.blocks.get <1> ().begin ()->arrival);
}

TEST (gap_cache, gap_bootstrap)
{
	rai::system system (24000, 2);
	rai::block_hash latest (system.nodes [0]->latest (rai::test_genesis_key.pub));
	rai::keypair key;
	rai::send_block send (latest, key.pub, rai::genesis_amount - 100, rai::test_genesis_key.prv, rai::test_genesis_key.pub, system.work.generate (latest));
	{
		rai::transaction transaction (system.nodes [0]->store.environment, nullptr, true);
		ASSERT_EQ (rai::process_result::progress, system.nodes [0]->process_receive_one (transaction, send).code);
	}
	ASSERT_EQ (rai::genesis_amount - 100, system.nodes [0]->balance (rai::genesis_account));
	ASSERT_EQ (rai::genesis_amount, system.nodes [1]->balance (rai::genesis_account));
	system.wallet (0)->insert_adhoc (rai::test_genesis_key.prv);
	system.wallet (0)->insert_adhoc (key.prv);
	system.wallet (0)->send_action (rai::test_genesis_key.pub, key.pub, 100);
	ASSERT_EQ (rai::genesis_amount - 200, system.nodes [0]->balance (rai::genesis_account));
	ASSERT_EQ (rai::genesis_amount, system.nodes [1]->balance (rai::genesis_account));
	auto iterations2 (0);
	while (system.nodes [1]->balance (rai::genesis_account) != rai::genesis_amount - 200)
	{
		system.poll ();
		++iterations2;
		ASSERT_LT (iterations2, 200);
	}
}

TEST (gap_cache, two_dependencies)
{
	rai::system system (24000, 1);
	rai::keypair key;
	rai::genesis genesis;
	rai::send_block send1 (genesis.hash (), key.pub, 1, rai::test_genesis_key.prv, rai::test_genesis_key.pub, system.work.generate (genesis.hash ()));
	rai::send_block send2 (send1.hash (), key.pub, 0, rai::test_genesis_key.prv, rai::test_genesis_key.pub, system.work.generate (send1.hash ()));
	rai::open_block open (send1.hash (), key.pub, key.pub, key.prv, key.pub, system.work.generate (key.pub));
	ASSERT_EQ (0, system.nodes [0]->gap_cache.blocks.size ());
	{
		rai::transaction transaction (system.nodes [0]->store.environment, nullptr, true);
		system.nodes [0]->process_receive_many (transaction, send2);
	}
	ASSERT_EQ (1, system.nodes [0]->gap_cache.blocks.size ());
	{
		rai::transaction transaction (system.nodes [0]->store.environment, nullptr, true);
		system.nodes [0]->process_receive_many (transaction, open);
	}
	ASSERT_EQ (2, system.nodes [0]->gap_cache.blocks.size ());
	{
		rai::transaction transaction (system.nodes [0]->store.environment, nullptr, true);
		system.nodes [0]->process_receive_many (transaction, send1);
	}
	ASSERT_EQ (0, system.nodes [0]->gap_cache.blocks.size ());
	rai::transaction transaction (system.nodes [0]->store.environment, nullptr, false);
	ASSERT_TRUE (system.nodes [0]->store.block_exists (transaction, send1.hash ()));
	ASSERT_TRUE (system.nodes [0]->store.block_exists (transaction, send2.hash ()));
	ASSERT_TRUE (system.nodes [0]->store.block_exists (transaction, open.hash ()));
}
