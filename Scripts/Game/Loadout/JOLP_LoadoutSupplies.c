//------------------------------------------------------------------------------------------------
//! Moves the net supply cost on the arsenal the player is standing at.
//! Positive net is a charge. Negative net is a refund. Zero does not touch the stock.
class JOLP_LoadoutSupplies
{
	//------------------------------------------------------------------------------------------------
	static int Read(IEntity arsenal)
	{
		SCR_ResourceConsumer consumer = Consumer(arsenal);
		if (!consumer)
			return 0;

		return Math.Round(consumer.GetAggregatedResourceValue());
	}

	//------------------------------------------------------------------------------------------------
	static bool CanSettle(IEntity arsenal, int net, out string error)
	{
		error = string.Empty;
		if (net == 0)
			return true;

		SCR_ResourceComponent resource = ResourceOf(arsenal);
		if (!resource)
		{
			error = "This arsenal has no supply stock.";
			return false;
		}

		if (net > 0)
		{
			SCR_ResourceConsumer consumer = Consumer(arsenal);
			if (!consumer)
			{
				error = "This arsenal has no supply stock.";
				return false;
			}

			if (consumer.GetAggregatedResourceValue() + 0.01 < net)
			{
				error = "Not enough supplies.";
				return false;
			}

			return true;
		}

		SCR_ResourceGenerator generator = Generator(resource);
		if (!generator)
		{
			error = "This arsenal cannot refund supplies.";
			return false;
		}

		SCR_ResourceConsumer consumer = Consumer(arsenal);
		if (consumer)
		{
			float available = consumer.GetAggregatedResourceValue();
			float maxValue = consumer.GetAggregatedMaxResourceValue();
			if (maxValue > 0 && available - net > maxValue + 0.01)
			{
				error = "This arsenal cannot hold the refund.";
				return false;
			}
		}

		return true;
	}

	//------------------------------------------------------------------------------------------------
	static bool Settle(IEntity arsenal, int net, out string error)
	{
		error = string.Empty;
		if (net == 0)
			return true;

		if (!CanSettle(arsenal, net, error))
			return false;

		SCR_ResourceComponent resource = ResourceOf(arsenal);
		if (net > 0)
		{
			SCR_ResourceConsumer consumer = Consumer(arsenal);
			float before = consumer.GetAggregatedResourceValue();
			consumer.RequestConsumtion(net);
			float spent = before - consumer.GetAggregatedResourceValue();
			if (spent + 0.5 < net)
			{
				if (spent > 0.5)
				{
					SCR_ResourceGenerator generator = Generator(resource);
					if (generator)
						generator.RequestGeneration(spent);
				}

				error = "Not enough supplies.";
				return false;
			}

			return true;
		}

		SCR_ResourceGenerator generator = Generator(resource);
		SCR_ResourceConsumer consumer = Consumer(arsenal);
		float before = 0;
		if (consumer)
			before = consumer.GetAggregatedResourceValue();

		generator.RequestGeneration(-net);
		if (consumer)
		{
			float gained = consumer.GetAggregatedResourceValue() - before;
			if (gained + 0.5 < -net)
			{
				error = "This arsenal cannot hold the refund.";
				return false;
			}
		}

		return true;
	}

	//------------------------------------------------------------------------------------------------
	protected static SCR_ResourceComponent ResourceOf(IEntity arsenal)
	{
		if (!arsenal)
			return null;

		return SCR_ResourceComponent.FindResourceComponent(arsenal);
	}

	//------------------------------------------------------------------------------------------------
	protected static SCR_ResourceConsumer Consumer(IEntity arsenal)
	{
		SCR_ResourceComponent resource = ResourceOf(arsenal);
		if (!resource)
			return null;

		SCR_ResourceConsumer consumer = resource.GetConsumer(EResourceGeneratorID.DEFAULT, EResourceType.SUPPLIES);
		if (consumer)
			return consumer;

		array<SCR_ResourceConsumer> consumers = resource.GetConsumers();
		if (!consumers || consumers.IsEmpty())
			return null;

		return consumers[0];
	}

	//------------------------------------------------------------------------------------------------
	protected static SCR_ResourceGenerator Generator(SCR_ResourceComponent resource)
	{
		if (!resource)
			return null;

		SCR_ResourceGenerator generator = resource.GetGenerator(EResourceGeneratorID.DEFAULT, EResourceType.SUPPLIES);
		if (generator)
			return generator;

		array<SCR_ResourceGenerator> generators = resource.GetGenerators();
		if (!generators || generators.IsEmpty())
			return null;

		return generators[0];
	}
}
